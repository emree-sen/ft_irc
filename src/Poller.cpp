#include "Poller.hpp"
#include <algorithm>
#include <unistd.h>

Poller::Poller() {}
Poller::~Poller() {}

void Poller::add(int fd, short events) {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) {
            return;
        }
    }
    struct pollfd p;
    p.fd = fd;
    p.events = events;
    p.revents = 0;
    _pfds.push_back(p);
}

void Poller::modify(int fd, short events) {
    for (size_t i = 0; i < _pfds.size(); ++i) if (_pfds[i].fd == fd) { _pfds[i].events = events; return; }
}

void Poller::remove(int fd) {
    for (size_t i = 0; i < _pfds.size(); ++i) if (_pfds[i].fd == fd) { _pfds.erase(_pfds.begin()+i); return; }
}

int Poller::wait(int timeoutMs) {
    nfds_t n = _pfds.empty() ? 0 : (nfds_t)_pfds.size();
    struct pollfd *ptr = _pfds.empty() ? (struct pollfd*)0 : &_pfds[0];
    return poll(ptr, n, timeoutMs);
}

bool Poller::isReadable(int fd) const {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd && (_pfds[i].revents & POLLIN)) {
            return true;
        }
    }
    return false;
}

bool Poller::isWritable(int fd) const {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd && (_pfds[i].revents & POLLOUT)) {
            return true;
        }
    }
    return false;
}

std::vector<int> Poller::readables() const {
    std::vector<int> out;
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].revents & POLLIN) {
            out.push_back(_pfds[i].fd);
        }
    }
    return out;
}

std::vector<int> Poller::writables() const {
    std::vector<int> out;
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].revents & POLLOUT) {
            out.push_back(_pfds[i].fd);
        }
    }
    return out;
}
