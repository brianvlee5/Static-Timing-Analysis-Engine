#include "LogicEvaluator.h"
#include <chrono>

//==========================================================
//  Helper function
//==========================================================

static LogicValue getLogicValue(GateType type, LogicValue A, LogicValue B) {
    // Helper function for propagation
    
    switch (type) {
        case GateType::INV:
            if (A == LogicValue::Zero) return LogicValue::One;
            if (A == LogicValue::One) return LogicValue::Zero;
            return LogicValue::Unknown; // Handles Unknown
            
        case GateType::NAND:
            // Strong Zero check: If A=1 and B=1, output is 0.
            if (A == LogicValue::One && B == LogicValue::One) return LogicValue::Zero;
            
            // Strong One check: If A=0 or B=0, output is 1, regardless of other input being X.
            if (A == LogicValue::Zero || B == LogicValue::Zero) return LogicValue::One;
            
            // Handles cases like (1, X), (X, 1), (X, X).
            return LogicValue::Unknown; 

        case GateType::NOR:
            // Strong One check: If A=0 and B=0, output is 1.
            if (A == LogicValue::Zero && B == LogicValue::Zero) return LogicValue::One;
            
            // Strong Zero check: If A=1 or B=1, output is 0, regardless of other input being X.
            if (A == LogicValue::One || B == LogicValue::One) return LogicValue::Zero;
            
            // Handles cases like (0, X), (X, 0), (X, X).
            return LogicValue::Unknown;
            
        case GateType::UNKNOWN:
        default:
            return LogicValue::Unknown;
    }
}

void LogicEvaluator::evaluateLogic() {
    
    // 1. Create and sort the topological order (C++11 compliant)
    std::vector<std::pair<std::string, int>> topo_order;
    for (const auto& pair : *gates_) {
        // Use Gate& g = pair.second; to access the gate object
        topo_order.emplace_back(pair.first, pair.second.topological_level);
    }

    using GateTopoPair = std::pair<std::string, int>;
    std::sort(topo_order.begin(), topo_order.end(), [](const GateTopoPair& a, const GateTopoPair& b) -> bool {
        return a.second < b.second;
    });

    // 2. Iterate over each input pattern
    
    // --- Reset all gate input pin logic values before processing new pattern ---
    // We only need to reset the destination pins
    for(auto& pair : *gates_) {
        Gate& gate = pair.second;
        gate.input_pin1.value = LogicValue::Unknown;
        gate.input_pin2.value = LogicValue::Unknown;
        gate.output_value = LogicValue::Unknown;
    }

    // --- 2.1 Set Primary Input (PI) signals (Fanout to load pins) ---
    for (const auto& signal : *pattern_) {
        auto net_it = nets_->find(signal.net_name);
        if (net_it != nets_->end()) {
            Net& pi_net = net_it->second;
            LogicValue pi_value = signal.value;

            // Fanout the PI value to all connected input pins on load gates
            for (Gate* load_gate : pi_net.loads) {
                // Check input_pin1
                if(load_gate->input_pin1.net){
                    if (load_gate->input_pin1.net->name == pi_net.name) {
                        load_gate->input_pin1.value = pi_value;
                    }
                }
                // Check input_pin2
                if(load_gate->input_pin2.net){
                    if (load_gate->input_pin2.net->name == pi_net.name) {
                        load_gate->input_pin2.value = pi_value;
                    }
                }
            }
        }
    }
    
    // --- 2.2 Propagate logic through topologically sorted gates ---
    for(const auto& gate_pair : topo_order) {
        Gate& current_gate = (*gates_)[gate_pair.first];
        
        // a. Read input pin values (These values must be set by the driver/PI logic)
        // We read directly from the Pin::value field
        LogicValue input_A = current_gate.input_pin1.value;
        LogicValue input_B = current_gate.input_pin2.value;

        // b. Evaluate the gate logic
        LogicValue output_Z = getLogicValue(current_gate.type, input_A, input_B);
        
        // c. Store the result on the gate's output
        current_gate.output_value = output_Z;
        current_gate.output_pin.value = output_Z;

        // d. Propagate the output value (output_Z) to ALL load gates (Fanout)
        Net* output_net = current_gate.output_pin.net;

        if (output_net) {
            for (Gate* load_gate : output_net->loads) {
                // Check input_pin1
                if(load_gate->input_pin1.net){
                    if (load_gate->input_pin1.net->name == output_net->name) {
                        load_gate->input_pin1.value = output_Z;
                    }
                }
                // Check input_pin2 (if it exists)
                if (load_gate->input_pin2.net) {
                    if (load_gate->input_pin2.net->name == output_net->name) {
                        load_gate->input_pin2.value = output_Z;
                    }
                }
            }
        }
    }
    

}

void LogicEvaluator::run() {
    evaluateLogic();
}