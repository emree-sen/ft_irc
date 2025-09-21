#ifndef POLLER_HPP
#define POLLER_HPP

#include <vector>
#include <poll.h>

class Poller {
public:
    Poller();
    ~Poller();

    void add(int fd, short events);
    void modify(int fd, short events);
    void remove(int fd);

    int wait(int timeoutMs = 1000);

    bool isReadable(int fd) const;
    bool isWritable(int fd) const;

    std::vector<int> readables() const;
    std::vector<int> writables() const;

private:
    std::vector<struct pollfd> _pfds;
};

#endif
