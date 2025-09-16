#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

static void handle_signal(int) {
    Server::signalStop();
}

static bool isNumber(const std::string &s) {
    for (size_t i = 0; i < s.size(); ++i) if (!std::isdigit(s[i])) return false;
    return !s.empty();
}

int main(int argc, char **argv) {
    std::signal(SIGINT, handle_signal);

    if (argc != 3) {
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }
    std::string portStr = argv[1];
    std::string password = argv[2];
    if (!isNumber(portStr)) {
        std::cerr << "Error: port must be a number" << std::endl;
        return 1;
    }
    int port = std::atoi(portStr.c_str());
    try {
        Server server(port, password);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
