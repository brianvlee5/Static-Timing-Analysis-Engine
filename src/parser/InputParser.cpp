#include "InputParser.h"

namespace {
    static inline std::string trim(const std::string& s) {
        size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;//skip left space
        if (b == s.size()) return {};
        size_t e = s.size() - 1;
        while (e > b && std::isspace(static_cast<unsigned char>(s[e]))) --e;//skip right space
        return s.substr(b, e - b + 1);
    }

    static inline std::string toLower(std::string s) {
        for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    // Replace commas with spaces and split by whitespace
    static inline std::vector<std::string> splitCommaOrSpace(const std::string& s) {
        std::string tmp;
        tmp.reserve(s.size());
        for (char c : s) tmp.push_back(c == ',' ? ' ' : c);
        std::vector<std::string> toks;
        std::istringstream iss(tmp);
        std::string tok;
        while (iss >> tok) toks.push_back(tok);
        return toks;
    }

    // Return true if token is composed only of digits
    static inline bool looksLikeIndexToken(const std::string& tok) {
        if (tok.empty()) return false;
        for (char c : tok) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        return true;
    }

    // Map token to LogicValue.
    static inline LogicValue tokenToLogicValue(const std::string& tok) {
        if (tok.empty()) return LogicValue::Unknown;
        if (tok.size() == 1) {
            char c = tok[0];
            if (c == '0') return LogicValue::Zero;
            if (c == '1') return LogicValue::One;
            if (c == 'x' || c == 'X') return LogicValue::Unknown;
            if (c == 'z' || c == 'Z' || c == '-') return LogicValue::Unknown;
        }
        return LogicValue::Unknown;
    }

} // namespace

void InputParser::parse(const std::string& filename,
                        std::vector<std::string>& inputNames,
                        std::vector<std::vector<Signal>>& patterns)
{    
    inputNames.clear();
    patterns.clear();

    std::ifstream ifs(filename);
    if (!ifs) return;

    std::string line;
    bool headerFound = false;

    // Read file line-by-line; first find header which contains the keyword "input"
    // Accept "input N1, N2, N3" or "1 input N1, N2, N3"
    while (std::getline(ifs, line)) {
        std::string t = trim(line);
        if (t.empty()) continue;
        // lowercase for searching while preserving original for extraction
        std::string low = toLower(t);

        // if the line contains ".end" before header, there are no patterns
        if (!t.empty() && t[0] == '.' && toLower(t).rfind(".end", 0) == 0) {
            // no header found, return empty results
            return;
        }

        // Find token "input" among whitespace-separated tokens
        std::istringstream iss(t);
        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);

        for (size_t i = 0; i < tokens.size(); ++i) {
            if (toLower(tokens[i]) == "input") {
                // remainder of the original line after the "input" keyword is the net list
                size_t pos = toLower(t).find("input");
                if (pos == std::string::npos) continue;
                size_t after = pos + 5; // length of "input"
                std::string remainder = trim(t.substr(after));
                inputNames = splitCommaOrSpace(remainder);
                headerFound = true;
                break;
            }
        }
        if (headerFound) break;
    }

    // didn't find header
    if (!headerFound) return;

    // Now read pattern lines until ".end" (case-insensitive) or EOF.
    while (std::getline(ifs, line)) {
        std::string t = trim(line);
        if (t.empty()) continue;

        //if detect .end, stop reading
        std::string low = toLower(t);
        if (!t.empty() && t[0] == '.') {
            if (low.rfind(".end", 0) == 0) break;
            // skip other dot-commands
            continue;
        }

        // Tokenize by whitespace (pattern lines may optionally start with an index)
        std::istringstream iss(t);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) tokens.push_back(token);//use the property of stream to split by whitespace
        if (tokens.empty()) continue;

        size_t startIdx = 0;
        if (tokens.size() == inputNames.size() + 1 && looksLikeIndexToken(tokens[0])) startIdx = 1;//if there is a index infront of input patterns
        if (tokens.size() - startIdx < inputNames.size()) {
            // Not enough values on this line; skip it
            continue;
        }

        std::vector<Signal> pat;
        pat.reserve(inputNames.size());
        for (size_t i = 0; i < inputNames.size(); ++i) {
            Signal s;
            s.net_name = inputNames[i];
            s.value = tokenToLogicValue(tokens[startIdx + i]);
            pat.push_back(std::move(s));
        }
        patterns.push_back(std::move(pat));
    }
}



