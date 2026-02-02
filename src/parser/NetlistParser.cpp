#include "NetlistParser.h"
#include <unordered_set>


static std::string removeComments(const std::string& s){
    std::string out;
    out.reserve(s.size());
    size_t i = 0, n = s.size();
    while(i < n){
        if(i + 1 < n && s[i] == '/' && s[i+1] == '/'){//skip the line
            i += 2;
            while (i < n && s[i] != '\n') ++i;
        } 
        else if(i + 1 < n && s[i] == '/' && s[i+1] == '*'){
            i += 2;
            while (i + 1 < n && !(s[i] == '*' && s[i+1] == '/')) ++i;//not yet end of comment
            if (i + 1 < n) i += 2;
        } 
        else{
            out.push_back(s[i++]);
        }
    }
    return out;
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); }); // cast to unsigned char!
    return s;
}

static inline std::string trim(std::string s) {
    auto not_space = [](int c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static bool isOutputPortName(const std::string& port) {
    if (port.empty()) return false;
    std::string p = port;
    for (auto &c : p) c = std::toupper((unsigned char)c);//find Z only in first alphabet
    return (p.rfind("Z", 0) == 0) || (p.rfind("ZN", 0) == 0) ||
           (p.rfind("Q", 0) == 0) || (p.find("OUT") != std::string::npos);
}

void NetlistParser::parse(const std::string& filename,
                          std::unordered_map<std::string, Gate>& gates,
                          std::unordered_map<std::string, Net>& nets,
                          DelayTable &nand_delay_table_,
                          DelayTable &nor_delay_table_,
                          DelayTable &inv_delay_table_,
                          std::string &module_name,
                          CircuitGraph &circuit_graph_) {

    std::ifstream ifs(filename);

    if (!ifs){
        std::cerr << "Error: cannot open netlist file " << filename << std::endl;
        return;
    }

    std::ostringstream ss_file;
    ss_file << ifs.rdbuf();
    std::string text = ss_file.str();

    text = removeComments(text);


    std::stringstream text_stream(text);
    std::string statement;
    size_t nextId = 1;

    // Regex for port connections (still used, but on smaller strings)
    std::regex port_re(R"(\.\s*(\w+)\s*\(\s*([^)]+)\s*\))");

    while (std::getline(text_stream, statement, ';')) {
        statement = trim(statement);
        if (statement.empty()) continue;

        std::stringstream ss(statement);
        std::string first_token;
        ss >> first_token;
        std::string lower_first = toLower(first_token);

        // --- 1. Handle Declarations (input/output/wire) ---
        if (lower_first == "input" || lower_first == "output" || lower_first == "wire") {
            std::string type = lower_first;

            std::string rest_of_line;
            std::getline(ss, rest_of_line, ';');

            // Normalize: turn commas into spaces so we can >> tokens cleanly
            for (char& c : rest_of_line) {
                if (c == ',') c = ' ';
            }


            std::stringstream ls(rest_of_line);
            std::string token;
            while (ls >> token) {          // skips spaces, tabs, newlines automatically
            
                if (nets.find(token) == nets.end()) {
                    Net net;
                    net.name = token;
                    nets[token] = std::move(net);
                }
            
                if (type == "input") {
                    circuit_graph_.primary_inputs.push_back(token);
                } else if (type == "output") {
                    circuit_graph_.primary_outputs.push_back(token);
                }
            }

        }
        // --- 2. Handle Module/Gate Instantiations (ROBUST) ---
        else {
            // Work on the whole original statement to handle "U8(.A1(...", "INV U8(...", etc.
            std::string stmt = statement;
        
            // Skip 'module' lines and capture module name
            {
                std::stringstream tss(stmt);
                std::string tok0;
                tss >> tok0;
                if (toLower(tok0) == "module") {
                    tss >> module_name;

                    size_t pos = module_name.find('(');
                    if (pos != std::string::npos) {
                        module_name = module_name.substr(0, pos);
                    }

                    module_name = trim(module_name);
                    continue;
                }
            }
        
            // Find the FIRST '(' and the LAST ')'
            size_t first_paren = stmt.find('(');
            size_t last_paren  = stmt.rfind(')');
            if (first_paren == std::string::npos || last_paren == std::string::npos || last_paren < first_paren) {
                // Malformed instantiation
                continue;
            }
        
            // Extract the token(s) before '(' to get either "TYPE INST" or just "INST"
            std::string before_paren = trim(stmt.substr(0, first_paren));
            // Split by whitespace
            std::stringstream bss(before_paren);
            std::vector<std::string> pre_tokens;
            {
                std::string tok;
                while (bss >> tok) pre_tokens.push_back(tok);
            }
        
            std::string type_token;   // e.g., "NAND2_X1"
            std::string inst;         // e.g., "U8"
            if (pre_tokens.size() >= 2) {
                // Typical: TYPE INST (...)
                type_token = pre_tokens[0];
                inst       = pre_tokens.back(); // in case of extra qualifiers
            } else if (pre_tokens.size() == 1) {
                // No explicit type, only instance given: "U8(..."
                inst = pre_tokens[0];
            } else {
                // Nothing before '(', malformed
                continue;
            }
        
            // Port list is the content between the parentheses
            std::string plist = stmt.substr(first_paren + 1, last_paren - first_paren - 1);
        
            // --- Build Gate object ---
            Gate gateObj;
            gateObj.name = inst;
        
            // Determine type:
            auto lower_stmt = toLower(stmt);
            auto lower_type_token = toLower(type_token);
        
            auto set_type_from = [&](const std::string& s) {
                if (s.find("nand") != std::string::npos) {
                    gateObj.type = GateType::NAND; gateObj.delay_table = &nand_delay_table_;
                    return true;
                } else if (s.find("nor") != std::string::npos) {
                    gateObj.type = GateType::NOR; gateObj.delay_table = &nor_delay_table_;
                    return true;
                } else if (s.find("inv") != std::string::npos || s.find("buf") != std::string::npos) {
                    gateObj.type = GateType::INV;  gateObj.delay_table = &inv_delay_table_;
                    return true;
                }
                return false;
            };
        
            bool typed = false;
            if (!type_token.empty()) {
                typed = set_type_from(lower_type_token);
            }
            if (!typed) {
                // Fall back to scanning the whole statement
                typed = set_type_from(lower_stmt);
            }
            if (!typed) {
                gateObj.type = GateType::UNKNOWN; // delay_table stays nullptr
            }
        
            gateObj.id = nextId++;
            auto it = gates.emplace(inst, std::move(gateObj)).first;
            Gate* gptr = &it->second;
        
            // --- Parse ports safely (don't assume delay_table exists) ---
            std::smatch pm;
            std::string::const_iterator pstart = plist.cbegin();
        
            // Helper: decide if a port name is an output when type is unknown
            auto is_probable_output = [](const std::string& p) {
                static const std::unordered_set<std::string> outs = {
                    "ZN","Z","Y","Q","O","OUT"
                };
                std::string up = p;
                // normalize to upper
                for (auto& c : up) c = std::toupper(static_cast<unsigned char>(c));
                return outs.find(up) != outs.end();
            };
        
            while (std::regex_search(pstart, plist.cend(), pm, port_re)) {
                std::string portName = trim(pm[1].str());
                std::string netName  = trim(pm[2].str());
                if (netName.empty()) { pstart = pm.suffix().first; continue; }
            
                // Create net if missing
                if (nets.find(netName) == nets.end()) {
                    Net net;
                    net.name = netName;
                    nets[netName] = std::move(net);
                }
            
                // Decide driver vs load
                bool is_output = false;
                if (gptr->delay_table) {
                    // Use library pin naming
                    if (portName == gptr->delay_table->output_pin_name) {
                        is_output = true;
                    }
                } else {
                    // Heuristic for unknown types
                    is_output = is_probable_output(portName);
                }
            
                if (is_output) {
                    Pin temp;
                    temp.net = &nets[netName];
                    gptr->output_pin = temp;
                    nets[netName].drivers.push_back(gptr);
                } else {
                    Pin temp;
                    temp.net = &nets[netName];
                    if (gptr->delay_table) {
                        // If we know pin names, set input caps accordingly
                        if (portName == gptr->delay_table->pin1_name) {
                            temp.input_capacitance = gptr->delay_table->p1_input_capacitance;
                            gptr->input_pin1 = temp;
                        } else if (portName == gptr->delay_table->pin2_name) {
                            temp.input_capacitance = gptr->delay_table->p2_input_capacitance;
                            gptr->input_pin2 = temp;
                        }
                    }
                    nets[netName].loads.push_back(gptr);
                }
            
                pstart = pm.suffix().first;
            }
        }
        
    }
    
}
