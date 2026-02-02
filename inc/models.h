#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <regex>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <cctype>

#define OUTPUT_LOAD 0.03 //fF
#define WIRE_DELAY 0.005 //ns
// Simple logic value instead of vector<bool>
enum class LogicValue : uint8_t { Unknown = 0, Zero = 1, One = 2 };

// Strongly-typed gate kinds
enum class GateType : uint8_t {
    UNKNOWN,
    INV,
    NAND,
    NOR
};

// Forward declarations
struct Net;
struct Gate;
struct DelayTable;

// Per-pin representation
struct Pin {
    Net* net = nullptr;          // connected net
    double arrival_time = 0.0;      // time of arrival at this pin (ns)
    double transition_time= 0.0;   // transition time / slew (ns)
    double input_capacitance = 0.0; // input pin capacitance (fF)
    LogicValue value = LogicValue::Unknown;
    // helpful convenience: pin name or index could be stored elsewhere
};

// Your model definitions go here
struct Gate{
    std::string name;
    GateType type = GateType::UNKNOWN;
    double output_load = 0.0;             // load driven by this gate (fF)
    double propagation_delay = 0.0;
    double output_transition = 0.0;
    double worst_prop_delay = 0.0;
    double worst_output_trans = 0.0;
    Pin input_pin1;
    Pin input_pin2;
    Pin output_pin;
    LogicValue output_value = LogicValue::Unknown;
    LogicValue worst_output_value = LogicValue::Unknown;
    DelayTable* delay_table;
    size_t id = 0;    // unique id assigned by netlist manager
    int topological_level = -1; // level in circuit for timing analysis
    int in_degree = 0; // number of incoming edges for topo sort

    void print() const {
        std::cout << "Gate Name: " << name 
        << ", Type: " << static_cast<int>(type) 
        << ", ID: " << id << std::endl;
        std::cout << "  Output Load: " << output_load << " fF"
        << ", Propagation Delay: " << propagation_delay << " ns"
        << ", Output Transition: " << output_transition << " ns" << std::endl;
    }
};


struct Net{
    std::string name;
    std::vector<Gate*> drivers;  // typically one driver
    std::vector<Gate*> loads;    // fanout gates

    void print() const {
        std::cout << "Net Name: " << name 
        << ", #Drivers: " << drivers.size() 
        << ", #Loads: " << loads.size() << std::endl;
    }
};



struct DelayTable{
    double p1_input_capacitance;
    double p2_input_capacitance;
    std::string pin1_name;
    std::string pin2_name;
    std::string output_pin_name;
    std::vector<double> input_transitions_axis;
    std::vector<double> output_loads_axis;
    std::vector<std::vector<double>> rise_delays;
    std::vector<std::vector<double>> fall_delays;
    std::vector<std::vector<double>> rise_transition;
    std::vector<std::vector<double>> fall_transition;
};

struct CircuitGraph{
    std::vector<std::vector<std::string>> adj_list; // adjacency list; size = #nodes
    std::vector<std::string> primary_inputs;
    std::vector<std::string> primary_outputs;
    std::unordered_map<std::string, int> name_to_id;//map gate name to adjacency list index
};


struct Signal {
    std::string net_name;
    LogicValue value = LogicValue::Unknown;
    void print() const {
        int print_value;
        switch(value){
            case LogicValue::Zero:
                print_value = 0;
                break;
            case LogicValue::One:
                print_value = 1;
                break;
            case LogicValue::Unknown:
            default:
                print_value = 'X';
                break;
        }

        std::cout << "Signal Net: " << net_name 
        << ", Value: " << print_value << std::endl;
    }
};


#endif // MODELS_H