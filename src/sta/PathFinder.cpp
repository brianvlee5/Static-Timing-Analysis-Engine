#include "PathFinder.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>


// ----------------------------------------------------------------------
//                        HELPER IMPLEMENTATION
// ----------------------------------------------------------------------
std::vector<std::string> PathFinder::backTrace(
    Gate* po_driver_gate, 
    bool trace_max,
    const std::unordered_map<std::string, MinTimingData>* min_timing_map) {
    std::vector<std::string> path;
    Gate* current_gate = po_driver_gate;

    while (current_gate) {
        Net* critical_input_net = nullptr;
        
        // Add the net driven by the current gate (if not the first net, 
        // which is already added at the start of the trace)
        //the net should be in the path
        if (path.empty() && current_gate->output_pin.net) {
            path.push_back(current_gate->output_pin.net->name);
        } 
        else if (!path.empty() && current_gate->output_pin.net && path.back() != current_gate->output_pin.net->name) {
            path.push_back(current_gate->output_pin.net->name);
        }
        
        if (trace_max) {
            // --- Longest Path Logic (Max Arrival Time) ---
            double arr1 = current_gate->input_pin1.arrival_time;
            double arr2 = current_gate->input_pin2.arrival_time;
            
            // The latest input pin determines the critical path.
            // Check for driver existence before comparing.
            bool has_driver1 = current_gate->input_pin1.net;
            bool has_driver2 = current_gate->input_pin2.net;

            if (has_driver1 && (!has_driver2 || arr1 >= arr2)) {
                critical_input_net = current_gate->input_pin1.net;
            } 
            else if (has_driver2 && (!has_driver1 || arr2 > arr1)) {
                critical_input_net = current_gate->input_pin2.net;
            }

        } 
        else {

            double arr1 = current_gate->input_pin1.arrival_time;
            double arr2 = current_gate->input_pin2.arrival_time;
            
            // The latest input pin determines the critical path.
            // Check for driver existence before comparing.
            bool has_driver1 = current_gate->input_pin1.net;
            bool has_driver2 = current_gate->input_pin2.net;

            if (has_driver1 && (!has_driver2 || arr1 < arr2)) {
                critical_input_net = current_gate->input_pin1.net;
            } 
            else if (has_driver2 && (!has_driver1 || arr2 <= arr1)) {
                critical_input_net = current_gate->input_pin2.net;
            }

        }

        if (!critical_input_net || critical_input_net->drivers.empty()) {
            // Reached a Primary Input (PI) net, so add the net and stop
            if (critical_input_net){
                path.push_back(critical_input_net->name);  
            }
            break; 
        }

        // Add the critical input net to the path
        path.push_back(critical_input_net->name);

        // Move to the driving gate (assuming single driver per net)
        current_gate = critical_input_net->drivers.front();
    }

    // Path was built backward (PO net -> PI net), so reverse it for output
    std::reverse(path.begin(), path.end());
    return path;
}


//will find the earliest net to arrive current gate so just back trace simply
std::unordered_map<std::string, MinTimingData> PathFinder::calculateMinArrivalTimes(
    const std::vector<std::pair<std::string, int>> topo_order) {
    std::unordered_map<std::string, MinTimingData> min_timing;
    

    //reset all gate input arrival times
    for(const auto& gate_pair : topo_order){
        Gate *gate = &(*gates_)[gate_pair.first];
        gate->input_pin1.arrival_time = std::numeric_limits<double>::max();
        gate->input_pin2.arrival_time = std::numeric_limits<double>::max();
    } 

    for(const auto& gate_pair : topo_order) {
        Gate& gate = (*gates_)[gate_pair.first];
        
        // 1. Calculate input arrival times (using already propagated Pin::arrival_time)
        double arr1_with_wire = gate.input_pin1.arrival_time;
        double arr2_with_wire = gate.input_pin2.arrival_time;

        // 2. Find the earliest input arrival time
        double min_arrival_time_at_input = std::numeric_limits<double>::max();
        Net* earliest_net = nullptr;

        // Only consider input pins that are actually driven
        bool has_driver1 = gate.input_pin1.net && !gate.input_pin1.net->drivers.empty();
        bool has_driver2 = gate.input_pin2.net && !gate.input_pin2.net->drivers.empty();
        
        if(gate.topological_level > 0) {
            if (has_driver1 && arr1_with_wire < min_arrival_time_at_input) {
                min_arrival_time_at_input = arr1_with_wire;
                earliest_net = gate.input_pin1.net;
            }
            if (has_driver2 && arr2_with_wire < min_arrival_time_at_input) {
                min_arrival_time_at_input = arr2_with_wire;
                earliest_net = gate.input_pin2.net;
            }
        }
        
        // If it's a PI, the net name doesn't matter for path tracing here, but we still need the time.
        if (min_arrival_time_at_input == std::numeric_limits<double>::max()) {
            min_arrival_time_at_input = 0.0; // Default for PIs if not initialized
        }


        // 4. Compute the minimum output arrival time
        double min_output_arrival_time = min_arrival_time_at_input + gate.worst_prop_delay;
        if(gate.topological_level > 0){
            min_output_arrival_time += WIRE_DELAY; // For PI gates, output arrival time is 0
        }

        // update the gates' arrival time connected to this gate's output net
        for(auto &load_gate : gate.output_pin.net->loads){
            // Update the load gate's input pin timing based on this gate's output
            std::cout << "Updating load gate: " << load_gate->name << " from gate: " << gate.name << " min output arrival time: " << load_gate->input_pin1.arrival_time << std::endl;
            std::cout << "Updating load gate: " << load_gate->name << " from gate: " << gate.name << " min output arrival time: " << load_gate->input_pin2.arrival_time << std::endl;
            if(load_gate->input_pin1.net->name == gate.output_pin.net->name){
                if(load_gate->input_pin1.arrival_time > min_output_arrival_time)
                    load_gate->input_pin1.arrival_time = min_output_arrival_time;
            }
            else if(load_gate->input_pin2.net->name == gate.output_pin.net->name){
                if(load_gate->input_pin2.arrival_time > min_output_arrival_time)
                    load_gate->input_pin2.arrival_time = min_output_arrival_time;
            }
            std::cout << "Updating load gate: " << load_gate->name << " from gate: " << gate.name << " min output arrival time: " << load_gate->input_pin1.arrival_time << std::endl;
            std::cout << "Updating load gate: " << load_gate->name << " from gate: " << gate.name << " min output arrival time: " << load_gate->input_pin2.arrival_time << std::endl;
        }

        // 5. Store result for back-tracing
        MinTimingData data;
        data.min_arrival_time = min_output_arrival_time;
        data.source_net = earliest_net;
        min_timing[gate.name] = data;
    }
    return min_timing;
}

// ----------------------------------------------------------------------
//                         PUBLIC IMPLEMENTATION
// ----------------------------------------------------------------------

void PathFinder::findLongestDelay() {
    longest_delay_ = 0.0;
    longest_path_.clear();
    Gate* longest_driver = nullptr;
    
    // 1. Find the PO with the largest worst_prop_delay
    //only this step need to loop through all primary outputs, because there will only be at most two drivers from then on
    for (const auto& po_net_name : circuit_graph_->primary_outputs) {//loop through all primary output nets(the drivers cuz there will be at most one driver)
        auto net_it = nets_->find(po_net_name);
        if (net_it == nets_->end() || net_it->second.drivers.empty()) continue;//if po net didn't connect to gate

        Gate* driver = net_it->second.drivers.front();
        
        // Driver's worst_prop_delay is the final arrival time at the output net.
        double max_pin_arrival;
        if(driver->input_pin1.arrival_time > driver->input_pin2.arrival_time){
            max_pin_arrival = driver->input_pin1.arrival_time;
        } 
        else {
            max_pin_arrival = driver->input_pin2.arrival_time;
        }

        if (driver->worst_prop_delay + max_pin_arrival > longest_delay_) {//will recalculate the longest delay, so even if timingpropagator added wire delay to po, answer is correct
            longest_delay_ = driver->worst_prop_delay + max_pin_arrival;
            longest_driver = driver;
        }
    }

    // 2. Back-trace the path
    if (longest_driver) {
        longest_path_ = backTrace(longest_driver, true);
    }
}

void PathFinder::findShortestDelay() {
    shortest_delay_ = std::numeric_limits<double>::max();
    shortest_path_.clear();
    Gate* shortest_driver = nullptr;

    // 1. Find the PO with the smallest worst_prop_delay
    //only this step need to loop through all primary outputs, because there will only be at most two drivers from then on
    for (const auto& po_net_name : circuit_graph_->primary_outputs) {//loop through all primary output nets(the drivers cuz there will be at most one driver)
        auto net_it = nets_->find(po_net_name);
        if (net_it == nets_->end() || net_it->second.drivers.empty()) continue;//if po net didn't connect to gate

        Gate* driver = net_it->second.drivers.front();
        
        // Driver's worst_prop_delay is the final arrival time at the output net.
        double max_pin_arrival;
        if(driver->input_pin1.arrival_time > driver->input_pin2.arrival_time){
            max_pin_arrival = driver->input_pin1.arrival_time;
        } 
        else {
            max_pin_arrival = driver->input_pin2.arrival_time;
        }

        if (driver->worst_prop_delay + max_pin_arrival < shortest_delay_) {//will recalculate the shortest delay, so even if timingpropagator added wire delay to po, answer is correct
            shortest_delay_ = driver->worst_prop_delay + max_pin_arrival;
            shortest_driver = driver;
        }
    }

    // 2. Back-trace the path
    if (shortest_driver) {
        shortest_path_ = backTrace(shortest_driver, true);
    }
}

void PathFinder::outputPath() {
    std::ofstream outfile;
    outfile.open(output_filename_);


    if(!outfile.is_open()) {
        std::cerr << "Error opening file for writing path output: " << output_filename_ << std::endl;
        return;
    }
    
    // Set precision
    outfile << std::fixed << std::setprecision(6);
    
    // Output Longest Delay Path
    outfile << "Longest delay = " << longest_delay_ << ", the path is: ";
    for (size_t i = 0; i < longest_path_.size(); ++i) {
        outfile << longest_path_[i];
        if (i < longest_path_.size() - 1) {
            outfile << " -> ";
        }
    }
    outfile << "\n";

    // Output Shortest Delay Path
    outfile << "Shortest delay = " << shortest_delay_ << ", the path is: ";
    for (size_t i = 0; i < shortest_path_.size(); ++i) {
        outfile << shortest_path_[i];
        if (i < shortest_path_.size() - 1) {
            outfile << " -> ";
        }
    }
    outfile << "\n";

    outfile.close();

}


void PathFinder::run(){
    findLongestDelay();
    findShortestDelay();
    outputPath();
}