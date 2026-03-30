#ifndef TIMINGPROPAGATOR_H 
#define TIMINGPROPAGATOR_H

#include "models.h"
#include <vector>
#include <string>
#include <fstream>

class TimingPropagator {
private:
    //get from last step
    std::unordered_map<std::string, Gate>* gates_;
    std::unordered_map<std::string, Net>* nets_;
    CircuitGraph* circuit_graph_;

    DelayTable nand_delay_table_;  
    DelayTable nor_delay_table_;  
    DelayTable inv_delay_table_;  

    std::string step2_output_filename_;
    std::string step4_output_filename_;

public:
    TimingPropagator() {};
    ~TimingPropagator() {};
    TimingPropagator(
        std::unordered_map<std::string, Gate>& gates, 
        std::unordered_map<std::string, Net>& nets, 
        CircuitGraph& circuit_graph,
        DelayTable& nand_delay_table_,
        DelayTable& nor_delay_table_,
        DelayTable& inv_delay_table_,
        std::string step2_output_filename_,
        std::string step4_output_filename_)
        : gates_(&gates), nets_(&nets), circuit_graph_(&circuit_graph), nand_delay_table_(nand_delay_table_), nor_delay_table_(nor_delay_table_), inv_delay_table_(inv_delay_table_), step2_output_filename_(step2_output_filename_), step4_output_filename_(step4_output_filename_) {}


    //step 2
    void runStep2();
    void findWorstTimings();
    void outputWorstTimings();

    //step 4
    void runStep4();
    void findLogicTimings();
    void outputLogicTimings();

};


#endif