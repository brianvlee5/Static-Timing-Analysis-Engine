#include "ArgumentParser.h"
#include <stdexcept>

void ArgumentParser::parse(char *argv[]){
    if (!argv) {
        throw std::invalid_argument("argv is null");
    }


    // reset any previous values
    netlist_file_.clear();
    lib_file_.clear();
    input_pattern_file_.clear();

    int i = 1; // skip program name (argv[0])
    while (argv[i] != nullptr) {
        std::string arg = argv[i];

        // -l / --lib (with separate arg, or -l<file>, or --lib=<file>)
        if (arg == "-l") {
            ++i;
            if (argv[i] == nullptr) {
                throw std::invalid_argument("Missing library file after " + arg);
            }
            lib_file_ = argv[i];
            ++i;
            continue;
        }
        else if (arg == "-i") {// -i / --input (with separate arg, or -i<file>, or --input=<file>)
            ++i;
            if (argv[i] == nullptr) {
                throw std::invalid_argument("Missing input patterns file after " + arg);
            }
            input_pattern_file_ = argv[i];
            ++i;
            continue;
        }
        else{
            if (argv[i] == nullptr) {
                throw std::invalid_argument("Missing netlist file after " + arg);
            }
            netlist_file_ = argv[i];
            ++i;
            continue;
        }
        ++i;
    }
}