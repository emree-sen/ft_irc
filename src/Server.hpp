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
    private:
        int _listenFd;
        int _port;
        std::string _password;

        Poller *_poller;

        std::map<int, Client*> _clients;
        std::map<std::string, Channel*> _channels;

        void setupSocket();
        void handleAccept();
        void handleReadable(int fd);
        void handleWritable(int fd);
    public:
        Server(int port, const std::string &password);
        ~Server();

        void run();

        const std::string &getPassword() const { return _password; }

        void addClient(int fd, const std::string &ip, int port);
        void removeClient(int fd, const std::string &reason);

        Client *getClient(int fd);
        Client *findClientByNick(const std::string &nick);

        Channel *getOrCreateChannel(const std::string &name);
        Channel *findChannel(const std::string &name);
        void removeChannelIfEmpty(const std::string &name);

        void sendToClient(int fd, const std::string &message);
        void broadcastToChannel(const std::string &chan, int fromFd, const std::string &msg, bool includeSender = false);
        void updatePollWrite(int fd);

        static bool stopFlag;
        static void signalStop();
};

#endif
