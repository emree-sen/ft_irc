#include "Parser.hpp"
#include <cctype>

static std::string trim(const std::string &s) {
    size_t i = 0; while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    size_t j = s.size(); while (j > i && (s[j-1] == ' ' || s[j-1] == '\t')) --j;
    return s.substr(i, j - i);
}

Message Parser::parse(const std::string &line) {
    Message m; std::string s = line;
    size_t pos = 0;
    if (!s.empty() && s[0] == ':') {
        size_t sp = s.find(' ');
        if (sp != std::string::npos) { m.prefix = s.substr(1, sp-1); pos = sp + 1; }
        else return m;
    }
    size_t sp = s.find(' ', pos);
    if (sp == std::string::npos) { m.command = s.substr(pos); return m; }
    m.command = s.substr(pos, sp - pos);
    pos = sp + 1;
    while (pos < s.size()) {
        if (s[pos] == ':') {
            m.params.push_back(s.substr(pos + 1));
            break;
        }
        size_t next = s.find(' ', pos);
        if (next == std::string::npos) { m.params.push_back(s.substr(pos)); break; }
        m.params.push_back(s.substr(pos, next - pos));
        pos = next + 1;
    }
    // trim params
    for (size_t i = 0; i < m.params.size(); ++i) m.params[i] = trim(m.params[i]);
    return m;
}
