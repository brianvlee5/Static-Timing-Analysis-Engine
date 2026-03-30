#ifndef LOADCALCULATOR_H
#define LOADCALCULATOR_H

#include "models.h"
#include <vector>
#include <string>
#include <fstream>

class LoadCalculator {
private:
    //non owning
    std::unordered_map<std::string, Gate> *gates_;
    std::unordered_map<std::string, Net> *nets_;
    CircuitGraph* circuit_graph_;
    std::string output_filename_;


public:
    LoadCalculator() {};
    ~LoadCalculator() {};
    LoadCalculator(std::unordered_map<std::string, Gate>& gates, std::unordered_map<std::string, Net>& nets, CircuitGraph& circuit_graph, std::string output_filename = "") 
        : gates_(&gates), nets_(&nets), circuit_graph_(&circuit_graph), output_filename_(output_filename) {}


    void run();
    //store the gate and net data
    void load_data(std::unordered_map<std::string, Gate>& gates, std::unordered_map<std::string, Net>& nets, CircuitGraph& circuit_graph, std::string output_filename = "") {
        gates_ = &gates;
        nets_ = &nets;
        circuit_graph_ = &circuit_graph;
        output_filename_ = output_filename;
    }

    //create circuit graphs for each connected component
    void createCircuitGraphs();

    //calculate the load for each gate and modify gate vector
    void calculateLoads();

    //output the load results to a file
    void outputLoads();

};


#endif