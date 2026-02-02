#ifndef ARGUMENTPARSER_H
#define ARGUMENTPARSER_H

#include <string>

class ArgumentParser{
private:
    std::string netlist_file_;
    std::string lib_file_;
    std::string input_pattern_file_;

public:
    ArgumentParser() {};
    ~ArgumentParser() {};

    void parse(char *argv[]);
    const std::string NetlistFile() const noexcept { return netlist_file_; };
    const std::string LibFile() const noexcept { return lib_file_; };
    const std::string InputPatternFile() const noexcept { return input_pattern_file_; };
};

#endif