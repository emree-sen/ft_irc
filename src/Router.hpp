#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>

class Server;

class Router {
public:
    explicit Router(Server *s): _s(s) {}
    void handle(int fd, const std::string &line);
private:
    Server *_s;
};

#endif
