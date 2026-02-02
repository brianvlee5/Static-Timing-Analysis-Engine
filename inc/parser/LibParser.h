#ifndef LIBPARSER_H
#define LIBPARSER_H

#include "models.h"
#include <vector>
#include <string>
#include <fstream>

class LibParser {

public:
    LibParser() {};
    ~LibParser() {};

    void parse(std::string filename, DelayTable& nand_table, DelayTable& nor_table, DelayTable& inv_table, std::string& lib_name);
};


#endif