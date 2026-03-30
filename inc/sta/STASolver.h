#ifndef STASOLVER_H
#define STASOLVER_H

#include "models.h"
#include "ArgumentParser.h"
#include "LoadCalculator.h"
#include "TimingPropagator.h"
#include "LogicEvaluator.h"
#include "PathFinder.h"
#include "InputParser.h"
#include "LibParser.h"
#include "NetlistParser.h"

#include <unordered_map>
#include <iostream>
#include <vector>
#include <string>

class STASolver {
private:
    std::unordered_map<std::string, Gate> gates_;
    std::unordered_map<std::string, Net> nets_;
    CircuitGraph circuit_graph_;
    DelayTable nand_delay_table_;  
    DelayTable nor_delay_table_;  
    DelayTable inv_delay_table_;  

    std::vector<std::vector<Signal>> patterns_;
    std::vector<std::string> input_names_;

    std::string netlist_file_;
    std::string lib_file_;
    std::string input_pattern_file_;

    std::string load_output_file_;
    std::string delay_output_file_;
    std::string path_output_file_;
    std::string gate_info_output_file_;

    std::string lib_name_, module_name_;

public:
    STASolver() {};
    ~STASolver() {};

    //api to outside
    void runSTA(char *argv[]);

    void parse(char *argv[]);
    void generateOutputFileName();
};

#endif