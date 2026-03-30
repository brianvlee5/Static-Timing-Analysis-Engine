#include "STASolver.h"
#include <chrono>

//helper functions
void STASolver::generateOutputFileName(){
    load_output_file_ = lib_name_ + "_" + module_name_ + "_load.txt";
    delay_output_file_ = lib_name_ + "_" + module_name_ + "_delay.txt";
    path_output_file_ = lib_name_ + "_" + module_name_ + "_path.txt";
    gate_info_output_file_ = lib_name_ + "_" + module_name_ + "_gate_info.txt";
}

void STASolver::runSTA(char *argv[]) {
    using namespace std::chrono;

    // ---- 1. Parse ----
    parse(argv);

    // ---- 2. Generate Output File Name ----
    generateOutputFileName();

    // ---- 3. Load Calculator ----
    LoadCalculator load_calculator(gates_, nets_, circuit_graph_, load_output_file_);
    load_calculator.run();


    // ---- 4. Timing Propagator Step2 ----
    TimingPropagator timing_propagator(
        gates_, nets_, circuit_graph_, 
        nand_delay_table_, nor_delay_table_, inv_delay_table_,
        delay_output_file_, gate_info_output_file_);
    timing_propagator.runStep2();

    // ---- 5. Path Finder ----
    PathFinder path_finder(gates_, nets_, circuit_graph_, path_output_file_);
    path_finder.run();

    // ---- 6. Logic Evaluator Initialization ----
    LogicEvaluator logic_evaluator(gates_, nets_, circuit_graph_, gate_info_output_file_);

    // ---- 7. Clear Gate Info File ----
    {
        std::ofstream outfile;
        if (gate_info_output_file_ != "") {
            outfile.open(gate_info_output_file_);
            outfile.close();
        }
    }

    // ---- 8. Pattern Loop ----
    for (auto &pattern : patterns_) {
        logic_evaluator.loadPattern(pattern);
        logic_evaluator.run();
        timing_propagator.runStep4();

    }

}

void STASolver::parse(char *argv[]){
    ArgumentParser argument_parser;
    NetlistParser netlist_parser;
    LibParser lib_parser;
    InputParser input_parser;

    //parse argument
    argument_parser.parse(argv);
    netlist_file_ = argument_parser.NetlistFile();
    lib_file_ = argument_parser.LibFile();
    input_pattern_file_ = argument_parser.InputPatternFile();

    //parse netlist
    lib_parser.parse(lib_file_, nand_delay_table_, nor_delay_table_, inv_delay_table_, lib_name_);
    netlist_parser.parse(netlist_file_, gates_, nets_, nand_delay_table_, nor_delay_table_, inv_delay_table_, module_name_, circuit_graph_);

    input_parser.parse(input_pattern_file_, input_names_, patterns_);

}
