#include "Channel.hpp"
#include <algorithm>

Channel::Channel(const std::string &name)
: _name(name), _limit(0), _modeI(false), _modeT(false), _modeK(false), _modeL(false) {}

Channel::~Channel() {}

void Channel::addMember(int fd) {
    if (!isMember(fd)) _members.push_back(fd);
}

void Channel::removeMember(int fd) {
    for (size_t i = 0; i < _members.size(); ++i) if (_members[i] == fd) { _members.erase(_members.begin()+i); break; }
    _ops.erase(fd);
    _invited.erase(fd);
}

bool Channel::isMember(int fd) const {
    for (size_t i = 0; i < _members.size(); ++i) {
        if (_members[i] == fd) {
            return true;
        }
    }
    return false;
}

void Channel::addOp(int fd) { _ops.insert(fd); }
void Channel::removeOp(int fd) { _ops.erase(fd); }
bool Channel::isOp(int fd) const { return _ops.count(fd) != 0; }

void Channel::invite(int fd) { _invited.insert(fd); }
bool Channel::isInvited(int fd) const { return _invited.count(fd) != 0; }
void Channel::revokeInvite(int fd) { _invited.erase(fd); }
