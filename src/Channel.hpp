#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <set>
#include <map>

class Channel {
public:
    Channel(const std::string &name);
    ~Channel();

    const std::string &getName() const { return _name; }

    // membership
    void addMember(int fd);
    void removeMember(int fd);
    bool isMember(int fd) const;
    const std::vector<int> &getMembers() const { return _members; }
    bool empty() const { return _members.empty(); }

    // operators
    void addOp(int fd);
    void removeOp(int fd);
    bool isOp(int fd) const;

    // topic/key/limit/modes
    void setTopic(const std::string &t) { _topic = t; }
    const std::string &getTopic() const { return _topic; }

    void setKey(const std::string &k) { _key = k; _modeK = !k.empty(); }
    const std::string &getKey() const { return _key; }

    void setLimit(int l) { _limit = l; _modeL = (l > 0); }
    int getLimit() const { return _limit; }

    void setInviteOnly(bool b) { _modeI = b; }
    bool isInviteOnly() const { return _modeI; }

    void setTopicRestricted(bool b) { _modeT = b; }
    bool isTopicRestricted() const { return _modeT; }

    // invites
    void invite(int fd);
    bool isInvited(int fd) const;
    void revokeInvite(int fd);

private:
    std::string _name;
    std::vector<int> _members;
    std::set<int> _ops;

    std::string _topic;
    std::string _key;
    int _limit; // 0 = unlimited

    bool _modeI; // invite-only
    bool _modeT; // topic restricted
    bool _modeK; // has key
    bool _modeL; // has limit

    std::set<int> _invited;
};

#endif // CHANNEL_HPP
