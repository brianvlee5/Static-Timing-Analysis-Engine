#ifndef LOGICEVALUATOR_H
#define LOGICEVALUATOR_H

#include "models.h"
#include <vector>
#include <string>
#include <fstream>

class LogicEvaluator {
private:
    //get from last step
    std::unordered_map<std::string, Gate> *gates_;
    std::unordered_map<std::string, Net> *nets_;
    CircuitGraph* circuit_graph_;

    std::vector<Signal> *pattern_;
    std::string output_filename_;


public:
    LogicEvaluator() {};
    ~LogicEvaluator() {};
    LogicEvaluator(
        std::unordered_map<std::string, Gate>& gates, 
        std::unordered_map<std::string, Net>& nets, 
        CircuitGraph& circuitGraph,
        std::string output_filename_)
       : gates_(&gates), nets_(&nets), circuit_graph_(&circuitGraph), output_filename_(output_filename_) {}

    void loadPattern(std::vector<Signal>& pattern){
        pattern_ = &pattern;
    }


    void evaluateLogic();
    void run();

};


#endif