#include "LoadCalculator.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

static int extract_gate_number(const std::string& gate_name) {
    if (gate_name.empty()) return 0;
    
    size_t start = 0;
    // Skip non-digit characters at the beginning (like 'g')
    while (start < gate_name.size() && !std::isdigit(gate_name[start])) {
        start++;
    }
    
    // Convert the remaining string (the number part) to an integer
    try {
        if (start < gate_name.size()) {
            return std::stoi(gate_name.substr(start));
        }
    } 
    catch (...) {
        // Handle conversion errors gracefully (e.g., if it's just "g")
        return 0; 
    }
    return 0;
}

void LoadCalculator::run() {
    createCircuitGraphs();
    calculateLoads();
    outputLoads();
}

void LoadCalculator::createCircuitGraphs() {
    // Implementation for creating circuit graphs for each connected component
    /*
    // the graph is single way */
    int g_idx = 0;
    for(auto &g: *gates_) {
        std::string temp;
        std::vector<std::string> adj_gates;
        Gate* gate = &g.second;
        temp = gate->name;
        if(gate->output_pin.net == nullptr) {
            circuit_graph_->adj_list.push_back(adj_gates);
            circuit_graph_->name_to_id[temp] = g_idx;
            g_idx++;
            continue;
        }
        for(auto &connected_gate: gate->output_pin.net->loads) {//for each load gate connected to the output net
            adj_gates.push_back(connected_gate->name);
        }
        circuit_graph_->adj_list.push_back(adj_gates);
        circuit_graph_->name_to_id[temp] = g_idx;

        g_idx++;
    }
}

void LoadCalculator::calculateLoads() {
    // Implementation for calculating the load for each gate
    // This is a placeholder implementation
    for(auto &g: *gates_) {//gates_ is a pointer to original gates vector
        double load = 0.0;
        Gate *gate = &g.second;
        int g_idx = circuit_graph_->name_to_id[gate->name];
        std::vector<std::string> adj_gates = circuit_graph_->adj_list[g_idx];

        for(auto &load_gate_name: adj_gates) {
            Gate* load_gate = &(*gates_)[load_gate_name];
            std::string load_net_name = gate->output_pin.net->name;
            // Sum the input capacitances of the load gates
            if(load_gate->input_pin1.net && load_gate->input_pin1.net->name == load_net_name && load_gate->input_pin1.input_capacitance > 0)
                load += load_gate->input_pin1.input_capacitance;
            if(load_gate->input_pin2.net && load_gate->input_pin2.net->name == load_net_name && load_gate->input_pin2.input_capacitance > 0)
                load += load_gate->input_pin2.input_capacitance;
        }
        gate->output_load = load;
    }

    for(auto &po: circuit_graph_->primary_outputs) {
        Net *po_net = &(*nets_)[po];
        for(auto &driver_gate: po_net->drivers) {
            driver_gate->output_load += OUTPUT_LOAD; // add output load
        }
    }

}

void LoadCalculator::outputLoads() {
    // Implementation for outputting the load results to a file
    std::vector<std::pair<std::string, std::string>> output_lines;
    
    // Set up the floating-point output format for precision and fixed notation
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6); // Use 6 decimal places for precision

    for(const auto &g: *gates_) {
        const Gate &gate = g.second;
        
        // Clear stringstream for next use
        ss.str("");
        ss.clear();

        // Format the line: "g1 0.017647"
        // Note: The example output shows the name and load value separated by a single space, 
        // with the name left-aligned (no leading space for g1, but one for g4).
        ss << gate.name << " " << gate.output_load;
        
        // Store the original gate name (for sorting) and the formatted line
        output_lines.push_back({gate.name, ss.str()});
    }

    // 2. Sort the collected lines by gate name
    using GateOutputPair = std::pair<std::string, std::string>;
    
    std::sort(output_lines.begin(), output_lines.end(), 
        [](const GateOutputPair& a, const GateOutputPair& b) -> bool {
            // Note: 'a' and 'b' are now correctly typed as references to the pair.
            
            // Extract the numeric value from the gate name (a.first)
            int num_a = extract_gate_number(a.first);
            int num_b = extract_gate_number(b.first);
            
            // Compare the numeric values
            return num_a < num_b; 
        });

    // 3. Output the sorted results to the file
    std::ofstream outfile;
    if(output_filename_ != "") {
        outfile.open(output_filename_);
    }

    if(!outfile.is_open()) {
        std::cerr << "Error opening file for writing load output: " 
                  << output_filename_ << std::endl;
        return;
    }

    for(const auto &p : output_lines) {
        // Output the already formatted string
        outfile << p.second << std::endl;
    }


    outfile.close();
}
