#include "Server.hpp"
#include "Poller.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Router.hpp"
#include "Utils.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <csignal>


// Kalma sebebi int flags
static void setNonBlocking(int fd) {
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl(F_SETFL) O_NONBLOCK failed");
    }
}

bool Server::stopFlag = false; // Initialize static stop flag

Server::Server(int port, const std::string &password)
: _listenFd(-1), _port(port), _password(password), _poller(0) {
    setupSocket();
    _poller = new Poller();
    _poller->add(_listenFd, POLLIN);
}

Server::~Server() {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        delete it->second;
    }
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        delete it->second;
    }
    if (_listenFd != -1) close(_listenFd);
    delete _poller;
}

void Server::setupSocket() {
    int yes = 1;
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0) throw std::runtime_error("socket() failed");
    setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    #ifdef SO_REUSEPORT
    setsockopt(_listenFd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
    #endif
    setNonBlocking(_listenFd);
    struct sockaddr_in addr4;
    std::memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_addr.s_addr = htonl(INADDR_ANY);
    addr4.sin_port = htons(_port);
    if (bind(_listenFd, (struct sockaddr*)&addr4, sizeof(addr4)) < 0) {
        close(_listenFd);
        throw std::runtime_error("bind() failed");
    }
    if (listen(_listenFd, 128) < 0) {
        close(_listenFd);
        throw std::runtime_error("listen() failed");
    }
    std::cout << "Listening on 0.0.0.0:" << _port << std::endl;
}

void Server::signalStop() {
    stopFlag = true;
}

void Server::run() {
    Router router(this);
    while (!stopFlag) {
        int n = _poller->wait();
        if (n < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, continue the loop
                continue;
            }
            // Log the error and throw an exception for other errors
            std::cerr << "poll() failed: " << strerror(errno) << std::endl;
            throw std::runtime_error("poll() failed");
        }
        if (n == 0) {
            // Timeout, continue the loop
            continue;
        }

        if (_poller->isReadable(_listenFd)) {
            handleAccept();
        }
        std::vector<int> rds = _poller->readables();
        for (size_t i = 0; i < rds.size(); ++i) {
            int fd = rds[i];
            if (fd == _listenFd) continue;
            handleReadable(fd);
        }
        std::vector<int> wrs = _poller->writables();
        for (size_t i = 0; i < wrs.size(); ++i) {
            int fd = wrs[i];
            if (fd == _listenFd) continue;
            handleWritable(fd);
        }
    }
}

void Server::handleAccept() {
    while (true) {
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        int cfd = accept(_listenFd, (struct sockaddr*)&ss, &slen);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "accept error: " << strerror(errno) << std::endl;
            break;
        }
        setNonBlocking(cfd);

        char ip[INET6_ADDRSTRLEN];
        int port = 0;
        if (ss.ss_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in*)&ss;
            inet_ntop(AF_INET, &s->sin_addr, ip, sizeof(ip));
            port = ntohs(s->sin_port);
        } else {
            struct sockaddr_in6 *s = (struct sockaddr_in6*)&ss;
            inet_ntop(AF_INET6, &s->sin6_addr, ip, sizeof(ip));
            port = ntohs(s->sin6_port);
        }

        addClient(cfd, ip, port);
    _poller->add(cfd, POLLIN);
    }
}

void Server::addClient(int fd, const std::string &ip, int port) {
    _clients[fd] = new Client(fd, ip, port);
    std::cout << "Client connected: " << ip << ":" << port << " (fd: " << fd << ")" << std::endl;
    // Hoşgeldin mesajı kaldırıldı - USER komutu tamamlandığında gönderilecek
}

void Server::removeClient(int fd, const std::string &reason) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client *c = it->second;
    std::cout << "Client disconnected: " << c->getPrefix() << " (" << reason << ")" << std::endl;
    // remove from channels
    std::vector<std::string> chans = c->getChannels();
    for (size_t i = 0; i < chans.size(); ++i) {
        Channel *ch = findChannel(chans[i]);
        if (ch) {
            ch->removeMember(fd);
            broadcastToChannel(ch->getName(), fd, ":" + c->getPrefix() + " PART " + ch->getName() + " :" + reason + "\r\n");
            removeChannelIfEmpty(ch->getName());
        }
    }
    _poller->remove(fd);
    close(fd);
    delete c;
    _clients.erase(it);
}

Client *Server::getClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return 0;
    return it->second;
}

Client *Server::findClientByNick(const std::string &nick) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNick() == nick) return it->second;
    }
    return 0;
}

Channel *Server::getOrCreateChannel(const std::string &name) {
    Channel *c = findChannel(name);
    if (!c) {
        c = new Channel(name);
        _channels[name] = c;
    }
    return c;
}

Channel *Server::findChannel(const std::string &name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it == _channels.end()) return 0;
    return it->second;
}

void Server::removeChannelIfEmpty(const std::string &name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end() && it->second->empty()) {
        delete it->second;
        _channels.erase(it);
    }
}

void Server::sendToClient(int fd, const std::string &message) {
    Client *c = getClient(fd);
    if (!c) return;
    c->queueWrite(message);
    updatePollWrite(fd);
}

void Server::broadcastToChannel(const std::string &chan, int fromFd, const std::string &msg, bool includeSender) {
    Channel *c = findChannel(chan);
    if (!c) return;
    const std::vector<int> &members = c->getMembers();
    for (size_t i = 0; i < members.size(); ++i) {
        int mfd = members[i];
        if (!includeSender && mfd == fromFd) continue;
        sendToClient(mfd, msg);
    }
}

void Server::handleReadable(int fd) {
    Client* c = getClient(fd);
    if (!c) return;

    char buf[4096];
    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            c->appendRead(buf, n);
        } else if (n == 0) {
            removeClient(fd, "Client quit");
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            removeClient(fd, "Read error");
            return;
        }
    }

    std::string line;
    Router router(this);
    while (true) {
        Client* c = getClient(fd);
        if (!c) break;
        if (!c->nextLine(line)) break;
        router.handle(fd, line);
    }
}

void Server::handleWritable(int fd) {
    Client *c = getClient(fd);
    if (!c) return;
    while (c->hasWrite()) {
        const std::string &front = c->peekWrite();
        ssize_t n = send(fd, front.c_str(), front.size(), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            removeClient(fd, "Write error");
            return;
        }
        c->consumeWrite(n);
        if (n == 0) break;
    }
    updatePollWrite(fd);
}

void Server::updatePollWrite(int fd) {
    Client *c = getClient(fd);
    if (!c) return;
    if (c->hasWrite()) _poller->modify(fd, POLLIN | POLLOUT);
    else _poller->modify(fd, POLLIN);
}
