#include "Client.hpp"
#include <algorithm>

Client::Client(int fd, const std::string &ip, int port)
: _fd(fd), _ip(ip), _port(port), _passOk(false) {}

Client::~Client() {}

std::string Client::getPrefix() const {
    std::string pre = _nick;
    if (!_user.empty()) pre += "!" + _user;
    pre += "@" + _ip;
    return pre;
}

void Client::appendRead(const char *data, size_t n) {
    _rbuf.append(data, n);
}

bool Client::nextLine(std::string &out) {
    size_t pos = _rbuf.find("\r\n");
    if (pos == std::string::npos) return false;
    out = _rbuf.substr(0, pos);
    _rbuf.erase(0, pos + 2);
    return true;
}

void Client::queueWrite(const std::string &msg) {
    _wq.push_back(msg);
}

bool Client::hasWrite() const { return !_wq.empty(); }

const std::string &Client::peekWrite() const { return _wq.front(); }

void Client::consumeWrite(size_t n) {
    if (_wq.empty()) return;
    if (n >= _wq.front().size()) {
        _wq.erase(_wq.begin());
    } else {
        _wq.front().erase(0, n);
    }
}

void Client::joinChannel(const std::string &name) {
    if (std::find(_channels.begin(), _channels.end(), name) == _channels.end()) _channels.push_back(name);
}

void Client::leaveChannel(const std::string &name) {
    for (size_t i = 0; i < _channels.size(); ++i) if (_channels[i] == name) { _channels.erase(_channels.begin()+i); return; }
}
