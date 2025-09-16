#include "Utils.hpp"

bool Utils::isChannelName(const std::string &s) {
    return !s.empty() && s[0] == '#';
}
