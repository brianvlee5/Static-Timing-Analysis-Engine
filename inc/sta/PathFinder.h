#ifndef PATHFINDER_H
#define PATHFINDER_H
#include "models.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include <sstream>
#include <iomanip>

struct MinTimingData {
    double min_arrival_time = std::numeric_limits<double>::max();
    Net* source_net = nullptr; // The net that provided the *earliest* arrival time
};

class PathFinder {
private:
    //get from last step
    std::unordered_map<std::string, Gate> *gates_;
    std::unordered_map<std::string, Net> *nets_;
    CircuitGraph* circuit_graph_;
    
    //owned
    std::vector<std::string> longest_path_;
    std::vector<std::string> shortest_path_;
    double longest_delay_ = 0.0;
    double shortest_delay_ = 0.0;

    std::string output_filename_;

    // Helper to back-trace the path (used for both max and min)
    std::vector<std::string> backTrace(Gate* po_driver_gate, bool trace_max, 
        const std::unordered_map<std::string, MinTimingData>* min_timing_map = nullptr);

    // Helper to perform the forward sweep for minimum delay calculation
    std::unordered_map<std::string, MinTimingData> calculateMinArrivalTimes(
        const std::vector<std::pair<std::string, int>> topo_order);

public:
    PathFinder() {};
    ~PathFinder() {};
    PathFinder(
        std::unordered_map<std::string, Gate>& gates, 
        std::unordered_map<std::string, Net>& nets, 
        CircuitGraph& circuit_graph,
        std::string output_filename_)
        : gates_(&gates), nets_(&nets), circuit_graph_(&circuit_graph), output_filename_(output_filename_) {}

    //store the gate and net data
    void load_data(
        std::unordered_map<std::string, Gate>& gates, 
        std::unordered_map<std::string, Net>& nets, 
        CircuitGraph& circuit_graph,
        std::string output_filename_){ 
        gates_ = &gates;
        nets_ = &nets;
        circuit_graph_ = &circuit_graph;
        output_filename_ = output_filename_;
    }

    void findLongestDelay();
    void findShortestDelay();
    void outputPath();

    void run();

};

#endif