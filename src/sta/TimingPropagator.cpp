#include "TimingPropagator.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <cassert>
#include <limits> // Required for numeric_limits
#include <chrono>

static void generateTopoLevelFast(std::unordered_map<std::string, Gate>* gates,
                                  std::unordered_map<std::string, Net>* nets,
                                  CircuitGraph* circuit_graph) {


    // 1) Reset levels and build indegrees
    // If you can, add a temporary field to Gate: int indegree_tmp;
    std::unordered_map<Gate*, int> indegree;
    indegree.reserve(gates->size());

    // For fast PI lookup
    // If primary_inputs is not an unordered_set<string>, consider building one.
    for (auto& g_pair : *gates) {
        Gate &g = g_pair.second;
        g.topological_level = -1;
        int d = 0;
        // Count how many of g's inputs are driven by other gates (not by PIs)
        if(g.input_pin1.net){
            Net* n1 = g.input_pin1.net;
            if (!n1->drivers.empty()) {
                ++d;
            }
        }
        if(g.input_pin2.net){
            Net* n2 = g.input_pin2.net;
            if (!n2->drivers.empty()) {
                ++d;    
            }
        }
        g.in_degree = d;
    }
    // 2) Seed queue with indegree-0 gates (driven only by PIs)
    std::deque<Gate*> q;
    q.clear();
    for (auto& g_pair : *gates) {
        Gate& g = g_pair.second;
        if (g.in_degree == 0) {
            g.topological_level = 0;
            q.push_back(&g);
        }
    }

    // 3) Kahn's traversal + longest-level DP
    size_t processed = 0;
    while (!q.empty()) {
        Gate* u = q.front(); q.pop_front();
        ++processed;

        Net* out = u->output_pin.net;
        if (!out) continue;

        for (Gate* v : out->loads) {
            // Relax longest path
            if (v->topological_level < u->topological_level + 1) {
                v->topological_level = u->topological_level + 1;
            }
            // One predecessor of v is now fully processed
            v->in_degree--;
            if (v->in_degree == 0) {
                q.push_back(v);
            }
        }
    }

    // 4) Optional: detect combinational loops
    if (processed != gates->size()) {
        std::cerr << "Warning: possible combinational cycle or missing PI/driver info; "
                     "levels may be incomplete.\n";
    }

}


static void generateTopoLevel(std::unordered_map<std::string, Gate>* gates,
                              std::unordered_map<std::string, Net>* nets,
                              CircuitGraph* circuit_graph) {
    using namespace std::chrono;

    auto start_total = high_resolution_clock::now();

    // ---- 1. Initialize topological levels ----
    auto start = high_resolution_clock::now();
    for (auto& pair : *gates) {
        Gate& gate = pair.second;
        gate.topological_level = -1;
    }
    auto end = high_resolution_clock::now();
    std::cout << "generateTopoLevel: initialization runtime: "
              << duration_cast<milliseconds>(end - start).count()
              << " ms" << std::endl;

    // ---- 2. Setup BFS queue (primary inputs) ----
    start = high_resolution_clock::now();
    std::vector<std::string> queue;
    for (const auto& pi : circuit_graph->primary_inputs) {
        for (auto* load_gate : (*nets)[pi].loads) {
            if (load_gate->topological_level < 0) {
                load_gate->topological_level = 0;
                queue.push_back(load_gate->name);
            }
        }
    }
    end = high_resolution_clock::now();
    std::cout << "generateTopoLevel: BFS setup runtime: "
              << duration_cast<milliseconds>(end - start).count()
              << " ms" << std::endl;

    // ---- 3. BFS traversal ----
    start = high_resolution_clock::now();
    while (!queue.empty()) {
        std::string current_gate_name = queue.front();
        queue.erase(queue.begin());
        Gate& current_gate = (*gates)[current_gate_name];
        int current_level = current_gate.topological_level;

        Net* output_net = current_gate.output_pin.net;

        if (output_net) {
            for (auto* load_gate : output_net->loads) {
                if (load_gate->topological_level < current_level + 1) {
                    load_gate->topological_level = current_level + 1;
                    queue.push_back(load_gate->name);
                }
            }
        }
    }
    end = high_resolution_clock::now();
    std::cout << "generateTopoLevel: BFS traversal runtime: "
              << duration_cast<milliseconds>(end - start).count()
              << " ms" << std::endl;

    // ---- 4. Total runtime ----
    auto end_total = high_resolution_clock::now();
    std::cout << "generateTopoLevel: total runtime: "
              << duration_cast<milliseconds>(end_total - start_total).count()
              << " ms" << std::endl;
}

static inline size_t nearestPairLowIndex(double x, const std::vector<double>& axis) {
    const size_t N = axis.size();
    assert(N >= 2);
    if (x <= axis.front()) return 0;          // 取 (0,1)
    if (x >= axis.back())  return N - 2;      // 取 (N-2, N-1)
    auto it = std::upper_bound(axis.begin(), axis.end(), x); // first > x
    size_t hi = size_t(it - axis.begin());
    return hi - 1; // lo，使得 axis[lo] <= x < axis[lo+1]
}

static inline double lerp(double a, double b, double t) { return a + t*(b - a); }

static double interpolate2D_swapped_axes_no_transpose(double C_actual, double T_actual,
                                                      const std::vector<double>& C_axis,  
                                                      const std::vector<double>& T_axis,  
                                                      const std::vector<std::vector<double>>& V) {
    const size_t CN = C_axis.size(), TN = T_axis.size();
    assert(CN >= 2 && TN >= 2);
    assert(V.size() == CN);
    for (const auto& r : V) assert(r.size() == TN);

    size_t r = nearestPairLowIndex(T_actual, T_axis); 
    size_t c = nearestPairLowIndex(C_actual, C_axis); 

    double t0 = T_axis[r],     t1 = T_axis[r+1];
    double c0 = C_axis[c],     c1 = C_axis[c+1];

    double u = (T_actual - t0) / (t1 - t0); 
    double s = (C_actual - c0) / (c1 - c0); 

    double f00 = V[r    ][c    ];
    double f10 = V[r + 1][c    ];
    double f01 = V[r    ][c + 1];
    double f11 = V[r + 1][c + 1];

    double f0 = lerp(f00, f10, u); 
    double f1 = lerp(f01, f11, u);
    return  lerp(f0,  f1,  s);     
}

static void calculateDelayAndProp(Gate* gate){
    if (!gate || !gate->delay_table) {
        printf("Error: Gate or its delay table is null.\n");
        return;
    }

    DelayTable* dt = gate->delay_table;

    // --- 1. Determine the Worst-Case Input Timing ---
    
    // We only need to consider the inputs that exist (pin1 and pin2)
    
    // The input arrival time includes the wire delay (0.005 ns)
    // Assuming Pin::arrival_time and Pin::transition_time have been updated 
    // from the driving gate's output timing in the calling loop.
    double arr1 = gate->input_pin1.arrival_time;
    double arr2 = gate->input_pin2.arrival_time;
    LogicValue logic_out = gate->output_value;
    
    // --- 1. Determine Critical Path (Min or Max Arrival Time) ---
    double critical_arrival_time = 0.0;
    double input_transition = 0.0;
    double min_arr = std::min(arr1, arr2);
    double max_arr = std::max(arr1, arr2);

    // Check which pin provides the latest signal (handles single-input gates if pin2 is zero/default)
    if(gate->type == GateType::INV){ 
        // INV: Always Max Criticality (latest input)
        critical_arrival_time = arr1; // Assuming Pin1 is the input for INV
        input_transition = gate->input_pin1.transition_time;
    }
    else if(gate->type == GateType::NAND){
        if (logic_out == LogicValue::One) { // see 
            if(gate->input_pin1.value == LogicValue::Zero && gate->input_pin2.value == LogicValue::Zero){
                // Both inputs are controlling (0), choose Min Criticality
                critical_arrival_time = min_arr;
                input_transition = (arr1 < arr2) ? gate->input_pin1.transition_time : gate->input_pin2.transition_time;
            }
            else if(gate->input_pin1.value == LogicValue::Zero && gate->input_pin2.value == LogicValue::One){
                // At least one input is X, choose Max Criticality
                critical_arrival_time = arr1;
                input_transition = gate->input_pin1.transition_time;
            }
            else{
                critical_arrival_time = arr2;
                input_transition = gate->input_pin2.transition_time;
            }
        } 
        else {
            critical_arrival_time = max_arr;
            input_transition = (arr1 > arr2) ? gate->input_pin1.transition_time : gate->input_pin2.transition_time;
        }
    }
    else if(gate->type == GateType::NOR){ 
        // Output LOW (Logic Zero): Transitioning AWAY from controlling value (1). Max Criticality.
        // Output HIGH (Logic One): Transitioning TO controlling value (1). Min Criticality.
        if (logic_out == LogicValue::One) {
            // Max Criticality (latest arrival time)
            critical_arrival_time = max_arr;
            input_transition = (arr1 > arr2) ? gate->input_pin1.transition_time : gate->input_pin2.transition_time;
        } 
        else {
            if(gate->input_pin1.value == LogicValue::One && gate->input_pin2.value == LogicValue::One){
                // Both inputs are controlling (1), choose Min Criticality
                critical_arrival_time = min_arr;
                input_transition = (arr1 < arr2) ? gate->input_pin1.transition_time : gate->input_pin2.transition_time;
            }
            else if(gate->input_pin1.value == LogicValue::One && gate->input_pin2.value == LogicValue::Zero){
                // At least one input is X, choose Max Criticality
                critical_arrival_time = arr1;
                input_transition = gate->input_pin1.transition_time;
            }
            else{
                critical_arrival_time = arr2;
                input_transition = gate->input_pin2.transition_time;
            }
        }
    }
    else {
        // Fallback for UNKNOWN gates: Use Max Criticality (safest assumption)
        critical_arrival_time = max_arr;
        input_transition = (arr1 >= arr2) ? gate->input_pin1.transition_time : gate->input_pin2.transition_time;
    }


    // If both are 0.0 (e.g., PI with 0.0 arrival), ensure transition is reasonable (maybe 0.0 or a library default)
    if (critical_arrival_time< 1e-9) { 
        critical_arrival_time = 0.0; // Primary Input starting time
        // Use a small default transition time if the circuit is just starting
        if (input_transition < 1e-9) {
             input_transition = 0.00; // Small default to prevent division by zero in interpolation
        }
    }


    // --- 2. Perform 2D Lookup for all 4 output values ---
    
    double C_actual = gate->output_load;
    double T_actual = input_transition;

    // 2.1 Calculate Rise Propagation Delay
    if(gate->output_value == LogicValue::One){
        gate->propagation_delay = interpolate2D_swapped_axes_no_transpose(C_actual, T_actual, 
                                    dt->output_loads_axis, 
                                    dt->input_transitions_axis, 
                                    dt->rise_delays);
        gate->output_transition = interpolate2D_swapped_axes_no_transpose(C_actual, T_actual, 
                                       dt->output_loads_axis, 
                                       dt->input_transitions_axis, 
                                       dt->rise_transition);
    }
    else if(gate->output_value == LogicValue::Zero){
        gate->propagation_delay = interpolate2D_swapped_axes_no_transpose(C_actual, T_actual, 
                                    dt->output_loads_axis, 
                                    dt->input_transitions_axis, 
                                    dt->fall_delays);
        gate->output_transition = interpolate2D_swapped_axes_no_transpose(C_actual, T_actual, 
                                       dt->output_loads_axis, 
                                       dt->input_transitions_axis, 
                                       dt->fall_transition);
    }
    else{
        gate->propagation_delay = 0.0;
        gate->output_transition = 0.0;
    }
    
    // Update the output pin's timing for downstream gates (loads)
    gate->output_pin.arrival_time = critical_arrival_time + gate->propagation_delay + WIRE_DELAY;
    gate->output_pin.transition_time = gate->output_transition;

    Net *output_net = gate->output_pin.net;
    if(output_net){
        for(auto* load_gate : output_net->loads){
            // Update the load gate's input pin timing based on this gate's output
            if(load_gate->input_pin1.net->name == output_net->name){
                load_gate->input_pin1.arrival_time = gate->output_pin.arrival_time;
                load_gate->input_pin1.transition_time = gate->output_pin.transition_time;
                load_gate->input_pin1.value = gate->output_value;
            }
            else if(load_gate->input_pin2.net->name == output_net->name){
                load_gate->input_pin2.arrival_time = gate->output_pin.arrival_time;
                load_gate->input_pin2.transition_time = gate->output_pin.transition_time;
                load_gate->input_pin2.value = gate->output_value;
            }
        }
    }

}

static void calculateDelayAndPropWorst(Gate* gate){
    if (!gate || !gate->delay_table) {
        printf("Error: Gate or its delay table is null.\n");
        return;
    }

    DelayTable* dt = gate->delay_table;

    // --- 1. Determine the Worst-Case Input Timing ---
    
    // We only need to consider the inputs that exist (pin1 and pin2)
    
    // The input arrival time includes the wire delay (0.005 ns)
    // Assuming Pin::arrival_time and Pin::transition_time have been updated 
    // from the driving gate's output timing in the calling loop.

    double arr1 = gate->input_pin1.arrival_time;
    double arr2 = gate->input_pin2.arrival_time;

    double max_arrival_time = 0.0;
    double worst_input_transition = 0.0;
    
    // Check which pin provides the latest signal (handles single-input gates if pin2 is zero/default)
    if (arr1 >= arr2) {
        max_arrival_time = arr1;
        worst_input_transition = gate->input_pin1.transition_time;
    } 
    else {
        max_arrival_time = arr2;
        worst_input_transition = gate->input_pin2.transition_time;
    }
    
    // If both are 0.0 (e.g., PI with 0.0 arrival), ensure transition is reasonable (maybe 0.0 or a library default)
    if (max_arrival_time < 1e-9) { 
        max_arrival_time = 0.0; // Primary Input starting time
        // Use a small default transition time if the circuit is just starting
        if (worst_input_transition < 1e-9) {
             worst_input_transition = 0.00; // Small default to prevent division by zero in interpolation
        }
    }


    // --- 2. Perform 2D Lookup for all 4 output values ---
    
    double C_actual = gate->output_load;
    double T_actual = worst_input_transition;

    // 2.1 Calculate Rise Propagation Delay
    double tpd_rise = interpolate2D_swapped_axes_no_transpose(C_actual, T_actual, 
                                    dt->output_loads_axis, 
                                    dt->input_transitions_axis, 
                                    dt->rise_delays);

    // 2.2 Calculate Fall Propagation Delay
    double tpd_fall = interpolate2D_swapped_axes_no_transpose(C_actual, T_actual, 
                                    dt->output_loads_axis, 
                                    dt->input_transitions_axis, 
                                    dt->fall_delays);

    // 2.3 Calculate Rise Output Transition Time
    double ttrans_rise = interpolate2D_swapped_axes_no_transpose(C_actual, T_actual, 
                                       dt->output_loads_axis, 
                                       dt->input_transitions_axis, 
                                       dt->rise_transition);

    // 2.4 Calculate Fall Output Transition Time
    double ttrans_fall = interpolate2D_swapped_axes_no_transpose(C_actual, T_actual, 
                                       dt->output_loads_axis, 
                                       dt->input_transitions_axis, 
                                       dt->fall_transition);

    
    // --- 3. Determine Worst-Case Output Timing ---
    
    // The propagation delay is the arrival time at the input pin PLUS the internal gate delay.

    if (tpd_rise >= tpd_fall) {
        // Worst case is rising (output = 1)
        gate->worst_prop_delay = tpd_rise;
        gate->worst_output_trans = ttrans_rise;
        gate->worst_output_value = LogicValue::One;
    } 
    else {
        // Worst case is falling (output = 0)
        gate->worst_prop_delay = tpd_fall;
        gate->worst_output_trans = ttrans_fall;
        gate->worst_output_value = LogicValue::Zero;
    }
    
    // Update the output pin's timing for downstream gates (loads)
    gate->output_pin.arrival_time = max_arrival_time + gate->worst_prop_delay + WIRE_DELAY;
    gate->output_pin.transition_time = gate->worst_output_trans;

    Net *output_net = gate->output_pin.net;
    if(output_net){
        for(auto* load_gate : output_net->loads){
            // Update the load gate's input pin timing based on this gate's output
            if(load_gate->input_pin1.net->name == output_net->name){
                load_gate->input_pin1.arrival_time = gate->output_pin.arrival_time;
                load_gate->input_pin1.transition_time = gate->output_pin.transition_time;
                load_gate->input_pin1.value = gate->worst_output_value;
            }
            else if(load_gate->input_pin2.net->name == output_net->name){
                load_gate->input_pin2.arrival_time = gate->output_pin.arrival_time;
                load_gate->input_pin2.transition_time = gate->output_pin.transition_time;
                load_gate->input_pin2.value = gate->worst_output_value;
            }
        }
    }

}


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


//==========================================================
// TimingPropagator Implementation
//==========================================================

void TimingPropagator::findWorstTimings() {
    std::vector<std::pair<std::string, int>> topo_order;
    // Create a topological order vector
    for (const auto& pair : *gates_) {
        const Gate& g = pair.second;
        topo_order.emplace_back(g.name, g.topological_level);
    }

    // Sort the topological order vector by level
    std::sort(topo_order.begin(), topo_order.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
        return a.second < b.second;
    });

    // Propagate timings in topological order
    for(const auto& gate_pair : topo_order) {
        const std::string& gate_name = gate_pair.first;
        Gate& gate = (*gates_)[gate_name];

        calculateDelayAndPropWorst(&gate);
    }

}

void TimingPropagator::outputWorstTimings() {
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
        std::string logic_value_str; 
        if(gate.worst_output_value == LogicValue::One) {
            logic_value_str = "1";
        } 
        else if(gate.worst_output_value == LogicValue::Zero) {
            logic_value_str = "0";
        } 
        else {
            logic_value_str = "X"; // Unknown
        }

        ss << gate.name << " " << logic_value_str << " " << gate.worst_prop_delay << " " << gate.worst_output_trans;
        
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
    if(step2_output_filename_ != "") {
        outfile.open(step2_output_filename_);
    }

    if(!outfile.is_open()) {
        std::cerr << "Error opening file for writing delay output: " 
                  << step2_output_filename_ << std::endl;
        return;
    }

    for(const auto &p : output_lines) {
        // Output the already formatted string
        outfile << p.second << std::endl;
    }


    outfile.close();

}



void TimingPropagator::runStep2() {
    generateTopoLevelFast(gates_, nets_, circuit_graph_);
    findWorstTimings();
    outputWorstTimings();

}

void TimingPropagator::findLogicTimings() {
    std::vector<std::pair<std::string, int>> topo_order;
    // Create a topological order vector
    for (const auto& pair : *gates_) {
        const Gate& g = pair.second;
        topo_order.emplace_back(g.name, g.topological_level);
    }

    // Sort the topological order vector by level
    std::sort(topo_order.begin(), topo_order.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
        return a.second < b.second;
    });

    // Propagate timings in topological order
    for(const auto& gate_pair : topo_order) {
        const std::string& gate_name = gate_pair.first;
        Gate& gate = (*gates_)[gate_name];

        calculateDelayAndProp(&gate);
    }
}

void TimingPropagator::outputLogicTimings() {
    std::vector<std::pair<std::string, std::string>> output_lines;
    
    // Set up the floating-point output format for precision and fixed notation
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6); // Use 6 decimal places for precision

    for(const auto &g: *gates_) {
        const Gate &gate = g.second;
        
        // Clear stringstream for next use
        ss.str("");
        ss.clear();
        std::string logic_value_str; 
        if(gate.output_value == LogicValue::One) {
            logic_value_str = "1";
        } 
        else if(gate.output_value == LogicValue::Zero) {
            logic_value_str = "0";
        } 
        else {
            logic_value_str = "X"; // Unknown
        }

        ss << gate.name << " " << logic_value_str << " " << gate.propagation_delay << " " << gate.output_transition;
        
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

    // 3. Output the sorted results to the file/
    std::ofstream outfile;
    if(step4_output_filename_ != "") {
        outfile.open(step4_output_filename_, std::ios::out | std::ios::app);
    }

    if(!outfile.is_open()) {
        std::cerr << "Error opening file for writing delay output: " 
                  << step4_output_filename_ << std::endl;
        return;
    }

    for(const auto &p : output_lines) {
        // Output the already formatted string
        outfile << p.second << std::endl;
    }
    outfile << std::endl; // Separate different runs with a blank line


    outfile.close();
}

void TimingPropagator::runStep4() {
    findLogicTimings();
    outputLogicTimings();
}