#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>

class Client {
public:
    Client(int fd, const std::string &ip, int port);
    ~Client();

    int getFd() const { return _fd; }
    const std::string &getNick() const { return _nick; }
    const std::string &getUser() const { return _user; }
    const std::string &getReal() const { return _real; }
    const std::string &getIP() const { return _ip; }
    int getPort() const { return _port; }
    const std::string &getHost() const { return _ip; }

    void setNick(const std::string &n) { _nick = n; }
    void setUser(const std::string &u) { _user = u; }
    void setReal(const std::string &r) { _real = r; }

    bool authed() const { return _passOk && !_nick.empty() && !_user.empty(); }
    void setPassOk(bool ok) { _passOk = ok; }
    bool isPassOk() const { return _passOk; }

    std::string getPrefix() const;

    void appendRead(const char *data, size_t n);
    bool nextLine(std::string &out);

    void queueWrite(const std::string &msg);
    bool hasWrite() const;
    const std::string &peekWrite() const;
    void consumeWrite(size_t n);

    void joinChannel(const std::string &name);
    void leaveChannel(const std::string &name);
    std::vector<std::string> getChannels() const { return _channels; }

private:
    int _fd;
    std::string _ip;
    int _port;

    std::string _nick, _user, _real;
    bool _passOk;

    std::string _rbuf;
    std::vector<std::string> _wq;

    std::vector<std::string> _channels;
};

#endif // CLIENT_HPP
