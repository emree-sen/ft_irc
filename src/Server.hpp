#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <poll.h>

class Client;
class Poller;
class Channel;
class Router;

class Server {
public:
    Server(int port, const std::string &password);
    ~Server();

    void run();

    // accessors
    const std::string &getPassword() const { return _password; }

    // client/channel management
    void addClient(int fd, const std::string &ip, int port);
    void removeClient(int fd, const std::string &reason);

    Client *getClient(int fd);
    Client *findClientByNick(const std::string &nick);

    Channel *getOrCreateChannel(const std::string &name);
    Channel *findChannel(const std::string &name);
    void removeChannelIfEmpty(const std::string &name);

    // message dispatch
    void sendToClient(int fd, const std::string &message);
    void broadcastToChannel(const std::string &chan, int fromFd, const std::string &msg, bool includeSender = false);
    void updatePollWrite(int fd);

    static bool stopFlag; // Declare static stop flag
    static void signalStop();

private:
    int _listenFd;
    int _port;
    std::string _password;

    Poller *_poller;

    std::map<int, Client*> _clients; // by fd
    std::map<std::string, Channel*> _channels; // by name

    void setupSocket();
    void handleAccept();
    void handleReadable(int fd);
    void handleWritable(int fd);
};

#endif // SERVER_HPP
