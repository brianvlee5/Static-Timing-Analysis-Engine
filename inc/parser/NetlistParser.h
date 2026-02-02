#ifndef NETLISTPARSER_H
#define NETLISTPARSER_H

#include "models.h"
#include <unordered_map>
#include <fstream>

class NetlistParser {
public:
    NetlistParser() {};
    ~NetlistParser() {};
    static void parse(const std::string& filename,
                      std::unordered_map<std::string, Gate>& gates,
                      std::unordered_map<std::string, Net>& nets,
                      DelayTable &nand_delay_table_,
                      DelayTable &nor_delay_table_,
                      DelayTable &inv_delay_table_,
                      std::string &module_name,
                      CircuitGraph &circuit_graph_); 

};



#endif