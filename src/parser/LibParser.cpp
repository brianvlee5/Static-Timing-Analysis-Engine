#include "LibParser.h" // adjust include as needed
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>

namespace { // internal helpers
    // Read entire file into string
    std::string readFile(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs) throw std::runtime_error("Failed to open file: " + path);
        std::ostringstream ss;
        ss << ifs.rdbuf();
        return ss.str();
    }

    // Remove C++-style //... and /* ... */ comments
    std::string removeComments(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        size_t i = 0, n = s.size();
        while (i < n) {
            if (i + 1 < n && s[i] == '/' && s[i+1] == '/') {
                i += 2;
                while (i < n && s[i] != '\n') ++i;
            } 
            else if (i + 1 < n && s[i] == '/' && s[i+1] == '*') {
                i += 2;
                while (i + 1 < n && !(s[i] == '*' && s[i+1] == '/')) ++i;
                if (i + 1 < n) i += 2;
            } 
            else {
                out.push_back(s[i++]);
            }
        }
        return out;
    }

    // Trim whitespace both ends
    static inline std::string trim(const std::string& s) {
        size_t b = 0;
        while (b < s.size() && std::isspace((unsigned char)s[b])) ++b;//left end
        if (b == s.size()) return {};
        size_t e = s.size() - 1;
        while (e > b && std::isspace((unsigned char)s[e])) --e;//right end
        return s.substr(b, e - b + 1);
    }

    // lower-case copy
    static inline std::string toLower(std::string s) {
        for (char &c : s) c = static_cast<char>(std::tolower((unsigned char)c));
        return s;
    }

    // find the content enclosed by matching braces (same depth) starting at pos of '{'
    // returns substring inside the outer braces and sets endPos to index after the matching '}'
    std::string extractBraceBlock(const std::string& text, size_t open_brace_pos, size_t& end_pos) {
        if (open_brace_pos >= text.size() || text[open_brace_pos] != '{')
            throw std::invalid_argument("extractBraceBlock: position not at '{'");
        size_t i = open_brace_pos;
        int depth = 0;
        ++i; // move past first '{'
        size_t start = i;
        while (i < text.size()) {
            if (text[i] == '{') {
                ++depth;
            } 
            else if (text[i] == '}') {
                if (depth == 0) {
                    end_pos = i + 1;
                    return text.substr(start, i - start);
                }
                --depth;
            }
            ++i;
        }
        throw std::runtime_error("Unbalanced braces in input");
    }

    // extract parentheses-block content starting at '(' (matching parentheses)
    // returns substring inside parentheses and sets endPos to index after closing ')'
    std::string extractParenBlock(const std::string& text, size_t openParenPos, size_t& endPos) {
        if (openParenPos >= text.size() || text[openParenPos] != '(')
            throw std::invalid_argument("extractParenBlock: position not at '('");
        size_t i = openParenPos;
        int depth = 0;
        ++i;
        size_t start = i;
        while (i < text.size()) {
            if (text[i] == '(') {
                ++depth;
            } else if (text[i] == ')') {
                if (depth == 0) {
                    endPos = i + 1;
                    return text.substr(start, i - start);
                }
                --depth;
            }
            ++i;
        }
        throw std::runtime_error("Unbalanced parentheses in input");
    }

    // Parse a quoted CSV block like: "a,b,c","d,e,f",... -> vector of rows, each row vector<double>
    std::vector<std::vector<double>> parseValuesBlock(const std::string& valuesContent) {
        // valuesContent is the text inside the parentheses of values(...) possibly containing quoted strings
        // We extract quoted substrings and parse each comma-separated number list.
        std::vector<std::vector<double>> rows;
        size_t i = 0, n = valuesContent.size();
        while (i < n) {
            // skip whitespace and commas before "0.1, 0.2, ...", stop when find """
            while (i < n && (std::isspace((unsigned char)valuesContent[i]) || valuesContent[i] == ',')) ++i;
            if (i >= n) break;
            if (valuesContent[i] == '"'){
                ++i;
                std::string q;
                while(i < n){//push content inside quotes
                    if(valuesContent[i] == '\\' && i + 1 < n){
                        // handle escaped characters: keep next char literally
                        q.push_back(valuesContent[i+1]);
                        i += 2;
                    } 
                    else if(valuesContent[i] == '"'){
                        ++i;
                        break;
                    } 
                    else{
                        q.push_back(valuesContent[i++]);
                    }
                }
                // q contains comma-separated numbers
                std::vector<double> row;
                std::string token;
                std::istringstream rs(q);
                while (std::getline(rs, token, ',')) {
                    token = trim(token);
                    if (token.empty()) continue;
                    double v = std::stod(token);
                    row.push_back(v);
                }
                rows.push_back(std::move(row));
            } 
            else{
                ++i;
            }
        }
        return rows;
    }

    // find first occurrence (case-insensitive) of needle in text starting from pos, returns npos if not found
    size_t findCaseInsensitive(const std::string& text, const std::string& needle, size_t pos = 0) {
        std::string lowText = text;
        std::string lowNeedle = toLower(needle);
        std::transform(lowText.begin() + pos, lowText.end(), lowText.begin() + pos, [](char c){ return static_cast<char>(std::tolower((unsigned char)c)); });
        size_t found = lowText.find(lowNeedle, pos);
        return found;
    }

    std::vector<double> parseCommaSeparatedDoubles(const std::string& data) {
        std::vector<double> results;
        std::stringstream ss(data);
        std::string segment;

        while (std::getline(ss, segment, ',')) {
            try {
                // Trim whitespace from segment before conversion (optional but safer)
                std::string trimmed_segment = trim(segment); 
                if (!trimmed_segment.empty()) {
                    results.push_back(std::stod(trimmed_segment));
                }
            } 
            catch (...) {
                // Handle conversion error if needed, but often skipped in parser implementations
            }
        }
        return results;
    }

} // anonymous namespace

// Implementation of LibParser::parse

void LibParser::parse(std::string filename, DelayTable& nand_table, DelayTable& nor_table, DelayTable& inv_table, std::string& lib_name) {
    // Read and preprocess file
    std::string text;
    try{
        text = readFile(filename);
    } 
    catch(const std::exception& e){
        // file error: leave tables empty
        return;
    }

    // Remove comments
    text = removeComments(text);

    // Remove backslash-newline continuations so values("a",\n "b") becomes values("a","b")
    {
        std::string tmp;
        tmp.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\\' && i + 1 < text.size() && (text[i+1] == '\n' || (text[i+1] == '\r' && i+2 < text.size() && text[i+2] == '\n'))){//ending is a "/" windows newline or linux
                // skip backslash and following newline (and possible CR)
                if (text[i+1] == '\r' && i+2 < text.size() && text[i+2] == '\n')
                    i += 2;
                else 
                    i += 1;
                continue;
            }
            tmp.push_back(text[i]);
        }
        text.swap(tmp);
    }

    //for finding library name
    {
        size_t libPos = findCaseInsensitive(text, "library", 0);
        if (libPos != std::string::npos) {
            size_t parenOpen = text.find('(', libPos);
            if (parenOpen != std::string::npos) {
                size_t parenEnd;
                try {
                    // extractParenBlock needs the original string, the starting '(' position, and an out reference for the end
                    std::string inside = extractParenBlock(text, parenOpen, parenEnd);
                    // Store the trimmed library name
                    lib_name = trim(inside);
                } 
                catch (...) {
                    // Failed to extract the block, leave lib_name empty
                }
            }
        }
    }

    // For lu table index
    std::string template_name;
    {
        size_t luPos = findCaseInsensitive(text, "lu_table_template", 0);
        if (luPos != std::string::npos) {

            // 1. Skip the template name block (e.g., skip (table10))
            size_t parenOpen = text.find('(', luPos);
            if (parenOpen == std::string::npos) return; 

            size_t parenEnd;
            try { 
                // extractParenBlock needs the original string, the starting '(' position, and an out reference for the end
                template_name = extractParenBlock(text, parenOpen, parenEnd); 
            } 
            catch (...) { 
                return; 
            }

            // 2. Find the content block starting with '{'
            size_t braceOpen = text.find('{', parenEnd);
            if (braceOpen == std::string::npos) return;

            size_t braceEnd;
            std::string innerBody;
            try {
                innerBody = extractBraceBlock(text, braceOpen, braceEnd);
            } 
            catch (...) { 
                return; 
            }
            // --- Dynamic Axis Mapping Logic ---
            std::unordered_map<std::string, std::vector<double>> index_values;
            std::string var1_type = ""; // Will store "LOAD" or "TRANSITION"
            std::string var2_type = ""; 
            
            // 3. Determine which variable maps to which physical quantity (LOAD or TRANSITION)
            // Regex: (variable_1 or variable_2) \s* : \s* (content up to semicolon)
            std::regex var_re(R"((variable_1|variable_2)\s*:\s*([^;]+);)", std::regex::icase);
            std::smatch vm;
            
            std::string::const_iterator var_start = innerBody.cbegin();
            while (std::regex_search(var_start, innerBody.cend(), vm, var_re)) {
                std::string var_name = vm[1].str();
                std::string var_content_lower = toLower(vm[2].str());
            
                // Check for key identifiers ("capacitance", "load" -> LOAD; "transition" -> TRANSITION)
                bool is_load_axis = (var_content_lower.find("capacitance") != std::string::npos || var_content_lower.find("load") != std::string::npos);
                
                if (var_name == "variable_1") {
                    var1_type = is_load_axis ? "LOAD" : "TRANSITION";
                } else if (var_name == "variable_2") {
                    var2_type = is_load_axis ? "LOAD" : "TRANSITION";
                }
            
                var_start = vm.suffix().first;
            }
            
            // 4. Parse Index Values (index_1 and index_2)
            // Regex: (index_1 or index_2) \s* ( \s* " (content) " \s* )
            std::regex index_re(R"((index_1|index_2)\s*\(\s*\"([^"]+)\"\s*\))", std::regex::icase);
            std::smatch m;
            std::string::const_iterator index_start = innerBody.cbegin();
            
            while (std::regex_search(index_start, innerBody.cend(), m, index_re)) {
                std::string index_name = m[1].str();
                std::string index_content = m[2].str();
            
                // Store the vector for later assignment
                index_values[index_name] = parseCommaSeparatedDoubles(index_content);
                
                index_start = m.suffix().first;
            }
            
            // 5. Assign the vectors to the correct DelayTable fields based on the variable mapping
            
            // Index 1 is intrinsically linked to Variable 1
            if (index_values.count("index_1")) {
                const auto& values = index_values.at("index_1");
                if (var1_type == "LOAD") {
                    nand_table.output_loads_axis = values;
                    nor_table.output_loads_axis = values;
                    inv_table.output_loads_axis = values;
                } else if (var1_type == "TRANSITION") {
                    nand_table.input_transitions_axis = values;
                    nor_table.input_transitions_axis = values;
                    inv_table.input_transitions_axis = values;
                }
            }
        
            // Index 2 is intrinsically linked to Variable 2
            if (index_values.count("index_2")) {
                const auto& values = index_values.at("index_2");
                if (var2_type == "LOAD") {
                    nand_table.output_loads_axis = values;
                    nor_table.output_loads_axis = values;
                    inv_table.output_loads_axis = values;
                } else if (var2_type == "TRANSITION") {
                    nand_table.input_transitions_axis = values;
                    nor_table.input_transitions_axis = values;
                    inv_table.input_transitions_axis = values;
                }
            }

        }
    }



    // Iterate over "cell (" occurrences
    size_t pos = 0;
    while (true) {
        // find "cell" (case-insensitive) followed by '('
        size_t cellPosLower = findCaseInsensitive(text, "cell", pos);
        if (cellPosLower == std::string::npos) break;
        // move to '(' after cell
        size_t parenPos = text.find('(', cellPosLower);
        if (parenPos == std::string::npos) break;

        // extract cell name inside parentheses
        size_t endParen;
        std::string cellName;
        try {
            std::string inside = extractParenBlock(text, parenPos, endParen);
            cellName = trim(inside);
        } 
        catch (...) {
            pos = parenPos + 1;
            continue;
        }

        // find the block starting with '{' after endParen
        size_t braceOpen = text.find('{', endParen);
        if (braceOpen == std::string::npos) break;
        size_t braceEnd;
        std::string cellBody;
        try {
            cellBody = extractBraceBlock(text, braceOpen, braceEnd);
        } 
        catch (...) {
            pos = braceOpen + 1;
            continue;
        }

        // move pos forward for next iteration
        pos = braceEnd;

        // determine which table we should populate
        std::string lname = toLower(cellName);//move to lower case for easier matching
        DelayTable* target = nullptr;
        if (lname.find("nand") != std::string::npos) {
            target = &nand_table;
        } 
        else if (lname.find("nor") != std::string::npos) {
            target = &nor_table;
        } 
        else if (lname.find("inv") != std::string::npos || lname.find("invt") != std::string::npos) {
            target = &inv_table;
        } 
        else {
            // skip other cell types
            continue;
        }

        // Reset target fields (clear all old data)
        target->p1_input_capacitance = 0.0;
        target->p2_input_capacitance = 0.0;
        target->pin1_name.clear();
        target->pin2_name.clear();
        target->output_pin_name.clear();
        target->rise_delays.clear();
        target->fall_delays.clear();
        target->rise_transition.clear();
        target->fall_transition.clear();

        // Pin counting for generic input assignment
        int input_pin_count = 0;
        std::string output_pin_body; // To store the body of the output pin block for later timing table parsing

        // Parse pin(...) blocks to find pin names, direction, and capacitances
        {
            size_t p = 0;
            while (true){
                size_t pinPos = findCaseInsensitive(cellBody, "pin(", p);
                if (pinPos == std::string::npos) break;
                size_t paren = cellBody.find('(', pinPos);
                if (paren == std::string::npos) break;
                size_t parenEnd;
                std::string pinName;
                try {
                    pinName = trim(extractParenBlock(cellBody, paren, parenEnd));
                } 
                catch (...) {
                    p = paren + 1;
                    continue;
                }
                // find the following { ... } block for this pin
                size_t brace = cellBody.find('{', parenEnd);
                if (brace == std::string::npos) break;
                size_t braceEndPin;
                std::string pinBody;
                try {
                    pinBody = extractBraceBlock(cellBody, brace, braceEndPin);
                } 
                catch (...) {
                    p = brace + 1;
                    continue;
                }

                // search for direction : value;
                std::regex dir_re(R"(\bdirection\b\s*:\s*([^;]+))", std::regex::icase);
                std::smatch dm;
                bool is_input = false;
                bool is_output = false;
                if (std::regex_search(pinBody, dm, dir_re)) {
                    std::string direction = trim(toLower(dm[1].str()));
                    if (direction.find("input") != std::string::npos) {
                        is_input = true;
                    } else if (direction.find("output") != std::string::npos) {
                        is_output = true;
                    }
                }

                if (is_input) {
                    // It's an input pin: extract capacitance and assign to p1 or p2
                    input_pin_count++;
                    std::regex cap_re(R"(\bcapacitance\b\s*:\s*([0-9eE+\-\.]+))", std::regex::icase);
                    std::smatch cm;
                    double c = 0.0;
                    if (std::regex_search(pinBody, cm, cap_re)) {
                        std::string val = cm[1].str();
                        try { c = std::stod(val); } catch(...) { c = 0.0; }
                    }

                    if (input_pin_count == 1) {
                        target->pin1_name = pinName;
                        target->p1_input_capacitance = c;
                    } 
                    else if (input_pin_count == 2) {
                        target->pin2_name = pinName;
                        target->p2_input_capacitance = c;
                    }
                } 
                else if (is_output) {
                    // It's the output pin: store its name and body for delay table parsing
                    target->output_pin_name = pinName;
                    output_pin_body = pinBody;
                }

                p = braceEndPin;
            }
        }
        
        // --- Delay Table Parsing (moved and updated) ---
        // Parse timing() block *within* the output pin body if an output pin was found
        if (target->output_pin_name.empty()) {
             // No output pin found, skip to next cell
             continue;
        }

        size_t timingPos = findCaseInsensitive(output_pin_body, "timing()", 0);
        if (timingPos == std::string::npos) {
            // no timing block inside output pin, skip
            continue;
        }
        // find the timing { } block
        size_t timingBrace = output_pin_body.find('{', timingPos);
        if (timingBrace == std::string::npos) continue;
        size_t timingEnd;
        std::string timingBody;
        try {
            timingBody = extractBraceBlock(output_pin_body, timingBrace, timingEnd);
        } 
        catch (...) {
            continue;
        }
        
        // The rest of the timing table parsing logic remains the same, 
        // operating on the extracted timingBody.

        // For each timing sub-block (cell_rise, cell_fall, rise_transition, fall_transition)
        auto parseTimingTable = [&](const std::string& timingName, std::vector<std::vector<double>>& outMatrix) {
            outMatrix.clear();
            size_t searchPos = 0;
            while (true) {
                std::string table_name;
                size_t tnPos = findCaseInsensitive(timingBody, timingName, searchPos);
                if (tnPos == std::string::npos) break;
                // locate '(' after timingName (e.g., cell_rise(table10){ ... })
                size_t tparen = timingBody.find('(', tnPos);
                if (tparen == std::string::npos) { searchPos = tnPos + timingName.size(); continue; }
                // skip the (...) and then find brace '{' for inner block
                size_t tparenEnd;
                try {
                    table_name = extractParenBlock(timingBody, tparen, tparenEnd);
                } 
                catch (...) {
                    searchPos = tparen + 1;
                    continue;
                }

                if(table_name != template_name) {
                    searchPos = tparenEnd;
                    continue;
                }

                size_t innerBrace = timingBody.find('{', tparenEnd);
                if (innerBrace == std::string::npos) { searchPos = tparenEnd; continue; }
                size_t innerEnd;
                std::string innerBody;
                try {
                    innerBody = extractBraceBlock(timingBody, innerBrace, innerEnd);
                } 
                catch (...) {
                    searchPos = innerBrace + 1;
                    continue;
                }
                // inside innerBody, find "values(" block and extract content
                size_t valuesPos = findCaseInsensitive(innerBody, "values", 0);
                if (valuesPos == std::string::npos) { searchPos = innerEnd; continue; }
                size_t vparen = innerBody.find('(', valuesPos);
                if (vparen == std::string::npos) { searchPos = valuesPos + 1; continue; }
                size_t vparenEnd;
                std::string valuesContent;
                try {
                    valuesContent = extractParenBlock(innerBody, vparen, vparenEnd);
                } 
                catch (...) {
                    searchPos = vparen + 1;
                    continue;
                }
                // parse quoted rows inside valuesContent
                auto rows = parseValuesBlock(valuesContent);
                // append rows to outMatrix (if multiple occurrences are found, concatenate)
                for (auto &r : rows) outMatrix.push_back(std::move(r));
                searchPos = innerEnd;
            }
        };

        // fill rise/fall delays and transitions
        parseTimingTable("cell_rise", target->rise_delays);
        parseTimingTable("cell_fall", target->fall_delays);
        parseTimingTable("rise_transition", target->rise_transition);
        parseTimingTable("fall_transition", target->fall_transition);
    } // end while over cells
}
