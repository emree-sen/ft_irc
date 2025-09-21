#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>

struct Message {
    std::string prefix;
    std::string command;
    std::vector<std::string> params;
};

class Parser {
public:
    static Message parse(const std::string &line);
};

#endif
