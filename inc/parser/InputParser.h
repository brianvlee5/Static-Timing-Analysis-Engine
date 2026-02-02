#ifndef INPUTPARSER_H
#define INPUTPARSER_H

#include "models.h"
#include <vector>
#include <string>
#include <fstream>

class InputParser {
public:
    InputParser() {};
    ~InputParser() {};

    void parse(const std::string& filename,
               std::vector<std::string>& inputNames,
               std::vector<std::vector<Signal>>& patterns);
};


#endif