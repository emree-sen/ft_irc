#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>

namespace Cmd {
    static const std::string PASS = "PASS";
    static const std::string NICK = "NICK";
    static const std::string USER = "USER";
    static const std::string PING = "PING";
    static const std::string PONG = "PONG";
    static const std::string QUIT = "QUIT";
    static const std::string JOIN = "JOIN";
    static const std::string PART = "PART";
    static const std::string PRIVMSG = "PRIVMSG";
    static const std::string MODE = "MODE";
    static const std::string TOPIC = "TOPIC";
    static const std::string INVITE = "INVITE";
    static const std::string KICK = "KICK";
}

#endif
