#include "Router.hpp"
#include "Parser.hpp"
#include "Command.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Utils.hpp"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <iostream>

static std::string crlf(const std::string &s) { return s + "\r\n"; }

void Router::handle(int fd, const std::string &line) {
    Message m = Parser::parse(line);
    // uppercase command
    for (size_t i = 0; i < m.command.size(); ++i) m.command[i] = std::toupper(m.command[i]);

    Client *c = _s->getClient(fd);
    if (!c) return;

    if (m.command == Cmd::PING) {
        if (m.params.empty()) { _s->sendToClient(fd, crlf(":ircserv 409 :No origin specified")); return; }
        _s->sendToClient(fd, crlf(":ircserv PONG :" + m.params[0]));
        return;
    }
    if (m.command == Cmd::PONG) return; // ignore

    if (m.command == Cmd::PASS) {
        if (m.params.empty()) { _s->sendToClient(fd, crlf(":ircserv 461 PASS :Not enough parameters")); return; }
        if (c->isPassOk()) { _s->sendToClient(fd, crlf(":ircserv 462 :You may not reregister")); return; }
        if (m.params[0] == _s->getPassword()) c->setPassOk(true);
        else { _s->sendToClient(fd, crlf(":ircserv 464 :Password incorrect")); }
        return;
    }
    if (m.command == Cmd::NICK) {
        if (m.params.empty()) { _s->sendToClient(fd, crlf(":ircserv 431 :No nickname given")); return; }
        std::string newNick = m.params[0];

        // Validate nickname format
        if (newNick.empty() || newNick.length() > 9) {
            _s->sendToClient(fd, crlf(":ircserv 432 * " + newNick + " :Erroneous nickname"));
            return;
        }

        // Check for invalid characters (space, tab, etc.)
        for (size_t i = 0; i < newNick.length(); ++i) {
            char c = newNick[i];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',') {
                _s->sendToClient(fd, crlf(":ircserv 432 * " + newNick + " :Erroneous nickname"));
                return;
            }
        }

        if (_s->findClientByNick(newNick)) { _s->sendToClient(fd, crlf(":ircserv 433 * " + newNick + " :Nickname is already in use")); return; }
        c->setNick(newNick);
        return;
    }
    if (m.command == Cmd::USER) {
        if (c->authed()) {
            _s->sendToClient(fd, crlf(":ircserv 462 :You may not reregister"));
            return;
        }
        if (m.params.size() < 4) { _s->sendToClient(fd, crlf(":ircserv 461 USER :Not enough parameters")); return; }
        c->setUser(m.params[0]);
        c->setReal(m.params[3]);
        if (!c->isPassOk()) {
            _s->sendToClient(fd, crlf(":ircserv 464 :Password required"));
        }
        else if (c->authed()) {
            // Client is now fully authenticated, send welcome messages
            _s->sendToClient(fd, crlf(":ircserv 001 " + c->getNick() + " :Welcome to the ft_irc network, " + c->getNick()));
            _s->sendToClient(fd, crlf(":ircserv 002 " + c->getNick() + " :Your host is ircserv, running version 1.0"));
            _s->sendToClient(fd, crlf(":ircserv 003 " + c->getNick() + " :This server was created sometime"));
            _s->sendToClient(fd, crlf(":ircserv 004 " + c->getNick() + " ircserv 1.0 io bikol"));
            std::cout << "Client authenticated: " << c->getPrefix() << std::endl;
        }
        return;
    }

    // Check if this is a known command that requires authentication
    bool isKnownCommand = (m.command == Cmd::QUIT || m.command == Cmd::JOIN ||
                          m.command == Cmd::PART || m.command == Cmd::PRIVMSG ||
                          m.command == Cmd::NOTICE || m.command == Cmd::MODE ||
                          m.command == Cmd::TOPIC || m.command == Cmd::INVITE ||
                          m.command == Cmd::KICK || m.command == Cmd::WHOIS);

    // If it's a known command that requires auth, check authentication
    if (isKnownCommand && !c->authed()) {
        _s->sendToClient(fd, crlf(":ircserv 451 :You have not registered"));
        return;
    }

    if (m.command == Cmd::QUIT) {
        std::vector<std::string> chans = c->getChannels();
        std::string quitReason = m.params.empty() ? "Client Quit" : m.params[0];
        std::string quitMsg = ":" + c->getPrefix() + " QUIT :" + quitReason;

        // Server log: User quit
        std::cout << "[SERVER] " << c->getNick() << " (" << c->getUser() << ") quit from server"
                  << " (reason: " << quitReason << ")";
        if (!chans.empty()) {
            std::cout << " [was in channels: ";
            for (size_t i = 0; i < chans.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << chans[i];
            }
            std::cout << "]";
        }
        std::cout << std::endl;

        for (size_t i = 0; i < chans.size(); ++i) {
            Channel *ch = _s->findChannel(chans[i]);
            if (ch) {
                _s->broadcastToChannel(ch->getName(), fd, crlf(quitMsg));
            }
        }
        _s->removeClient(fd, "Quit");
        return;
    }

    if (m.command == Cmd::JOIN) {
        if (m.params.empty()) { _s->sendToClient(fd, crlf(":ircserv 461 JOIN :Not enough parameters")); return; }
        std::string chanName = m.params[0];
        if (chanName.empty() || chanName[0] != '#') { _s->sendToClient(fd, crlf(":ircserv 476 :Bad Channel Mask")); return; }
        Channel *ch = _s->getOrCreateChannel(chanName);
        // very basic mode checks (key/invite/limit)
        if (!ch->getKey().empty()) {
            if (m.params.size() < 2 || m.params[1] != ch->getKey()) {
                _s->sendToClient(fd, crlf(":ircserv 475 " + chanName + " :Cannot join channel (+k)")); return;
            }
        }
        if (ch->isInviteOnly() && !ch->isInvited(fd)) {
            _s->sendToClient(fd, crlf(":ircserv 473 " + chanName + " :Cannot join channel (+i)")); return;
        }
        // Check channel limit only if user is not invited
        if (ch->getLimit() > 0 && (int)ch->getMembers().size() >= ch->getLimit() && !ch->isInvited(fd)) {
            _s->sendToClient(fd, crlf(":ircserv 471 " + chanName + " :Channel is full")); return;
        }
        ch->addMember(fd);
        c->joinChannel(chanName);
        if (ch->getMembers().size() == 1) ch->addOp(fd); // first member is op
        std::string joinMsg = ":" + c->getPrefix() + " JOIN " + chanName;
        _s->sendToClient(fd, crlf(joinMsg));
        _s->broadcastToChannel(chanName, fd, crlf(joinMsg));
        if (!ch->getTopic().empty()) _s->sendToClient(fd, crlf(":ircserv 332 " + c->getNick() + " " + chanName + " :" + ch->getTopic()));

        // Send RPL_NAMREPLY with channel members
        std::string members;
        const std::vector<int> &fds = ch->getMembers();
        for (size_t i = 0; i < fds.size(); ++i) {
            Client *member = _s->getClient(fds[i]);
            if (member) {
                if (!members.empty()) members += " ";
                members += member->getNick();
            }
        }
        _s->sendToClient(fd, crlf(":ircserv 353 " + c->getNick() + " = " + chanName + " :" + members));
        _s->sendToClient(fd, crlf(":ircserv 366 " + c->getNick() + " " + chanName + " :End of /NAMES list"));
        return;
    }

    if (m.command == Cmd::PART) {
        if (m.params.empty()) { _s->sendToClient(fd, crlf(":ircserv 461 PART :Not enough parameters")); return; }
        std::string chanName = m.params[0];
        Channel *ch = _s->findChannel(chanName);
        if (!ch || !ch->isMember(fd)) { _s->sendToClient(fd, crlf(":ircserv 442 " + chanName + " :You're not on that channel")); return; }
        ch->removeMember(fd);
        c->leaveChannel(chanName);

        // Server log: User left channel
        std::string partReason = (m.params.size() >= 2) ? m.params[1] : "No reason given";
        std::cout << "[SERVER] " << c->getNick() << " (" << c->getUser() << ") left channel "
                  << chanName << " (reason: " << partReason << ")" << std::endl;

        std::string partMsg = ":" + c->getPrefix() + " PART " + chanName + (m.params.size()>=2?" :"+m.params[1]:"");
        _s->broadcastToChannel(chanName, fd, crlf(partMsg));
        _s->sendToClient(fd, crlf(partMsg));
        return;
    }

    if (m.command == Cmd::PRIVMSG || m.command == Cmd::NOTICE) {
        if (m.params.empty()) {
            _s->sendToClient(fd, crlf(":ircserv 411 " + m.command + " :No recipient given"));
            return;
        }
        if (m.params.size() < 2) {
            _s->sendToClient(fd, crlf(":ircserv 412 :No text to send"));
            return;
        }
        std::string target = m.params[0];
        std::string text = m.params[1];
        std::string prefix = ":" + c->getPrefix() + " " + m.command + " " + target + " :" + text;
        if (!target.empty() && target[0] == '#') {
            Channel *ch = _s->findChannel(target);
            if (!ch || !ch->isMember(fd)) {
                _s->sendToClient(fd, crlf(":ircserv 404 " + target + " :Cannot send to channel"));
                return;
            }
            _s->broadcastToChannel(target, fd, crlf(prefix));
        } else {
            Client *dst = _s->findClientByNick(target);
            if (!dst) {
                _s->sendToClient(fd, crlf(":ircserv 401 " + target + " :No such nick"));
                return;
            }
            _s->sendToClient(dst->getFd(), crlf(prefix));
        }
        return;
    }

    if (m.command == Cmd::TOPIC) {
        if (m.params.empty()) { _s->sendToClient(fd, crlf(":ircserv 461 TOPIC :Not enough parameters")); return; }
        std::string chan = m.params[0];
        Channel *ch = _s->findChannel(chan);
        if (!ch || !ch->isMember(fd)) { _s->sendToClient(fd, crlf(":ircserv 442 " + chan + " :You're not on that channel")); return; }
        if (m.params.size() == 1) { // get topic
            if (ch->getTopic().empty()) _s->sendToClient(fd, crlf(":ircserv 331 " + chan + " :No topic is set"));
            else _s->sendToClient(fd, crlf(":ircserv 332 " + chan + " :" + ch->getTopic()));
            return;
        }
        if (ch->isTopicRestricted() && !ch->isOp(fd)) { _s->sendToClient(fd, crlf(":ircserv 482 " + chan + " :You're not channel operator")); return; }
        ch->setTopic(m.params[1]);
        std::string msg = ":" + c->getPrefix() + " TOPIC " + chan + " :" + m.params[1];
        _s->broadcastToChannel(chan, fd, crlf(msg));
        _s->sendToClient(fd, crlf(msg));
        return;
    }

    if (m.command == Cmd::INVITE) {
        if (m.params.size() < 2) { _s->sendToClient(fd, crlf(":ircserv 461 INVITE :Not enough parameters")); return; }
        std::string nick = m.params[0];
        std::string chan = m.params[1];
        Channel *ch = _s->findChannel(chan);
        if (!ch || !ch->isMember(fd)) { _s->sendToClient(fd, crlf(":ircserv 442 " + chan + " :You're not on that channel")); return; }
        if (!ch->isOp(fd)) { _s->sendToClient(fd, crlf(":ircserv 482 " + chan + " :You're not channel operator")); return; }
        Client *dst = _s->findClientByNick(nick);
        if (!dst) { _s->sendToClient(fd, crlf(":ircserv 401 " + nick + " :No such nick")); return; }
        ch->invite(dst->getFd());
        _s->sendToClient(fd, crlf(":ircserv 341 " + nick + " " + chan));
        _s->sendToClient(dst->getFd(), crlf(":ircserv INVITE " + nick + " :" + chan));
        return;
    }

    if (m.command == Cmd::KICK) {
        if (m.params.size() < 2) { _s->sendToClient(fd, crlf(":ircserv 461 KICK :Not enough parameters")); return; }
        std::string chan = m.params[0];
        std::string nick = m.params[1];
        Channel *ch = _s->findChannel(chan);
        if (!ch || !ch->isMember(fd)) {
            _s->sendToClient(fd, crlf(":ircserv 442 " + chan + " :You're not on that channel")); return;
        }
        if (!ch->isOp(fd)) {
            _s->sendToClient(fd, crlf(":ircserv 482 " + chan + " :You're not channel operator")); return;
        }
        Client *dst = _s->findClientByNick(nick);
        if (!dst || !ch->isMember(dst->getFd())) {
            _s->sendToClient(fd, crlf(":ircserv 441 " + nick + " " + chan + " :They aren't on that channel")); return;
        }

        std::string msg = ":" + c->getPrefix() + " KICK " + chan + " " + nick + (m.params.size()>=3?" :"+m.params[2]:"");

        // Send KICK message to all channel members manually
        const std::vector<int> &members = ch->getMembers();
        for (size_t i = 0; i < members.size(); ++i) {
            _s->sendToClient(members[i], crlf(msg));
        }

        // Now remove the user from the channel
        ch->removeMember(dst->getFd());
        dst->leaveChannel(chan);
        return;
    }

    if (m.command == Cmd::MODE) {
        if (m.params.empty()) {
            _s->sendToClient(fd, crlf(":ircserv 461 MODE :Not enough parameters"));
            return;
        }
        std::string target = m.params[0];
        if (!target.empty() && target[0] == '#') {
            Channel *ch = _s->findChannel(target);
            if (!ch) {
                _s->sendToClient(fd, crlf(":ircserv 403 " + target + " :No such channel"));
                return;
            }
            if (m.params.size() == 1) {
                std::string modes = "+";
                if (ch->isInviteOnly()) modes += 'i';
                if (ch->isTopicRestricted()) modes += 't';
                if (!ch->getKey().empty()) modes += 'k';
                if (ch->getLimit() > 0) modes += 'l';
                _s->sendToClient(fd, crlf(":ircserv 324 " + target + " " + modes));
                return;
            }
            bool add = true; // Initialize add for mode handling
            size_t pidx = 2; // Initialize parameter index for mode arguments
            std::string flags = m.params[1]; // Extract mode flags
            for (size_t i = 0; i < flags.size(); ++i) {
                char f = flags[i];
                if (f == '+') { add = true; continue; }
                if (f == '-') { add = false; continue; }
                if (f == 'i') ch->setInviteOnly(add);
                else if (f == 't') ch->setTopicRestricted(add);
                else if (f == 'k') {
                    if (add) {
                        if (pidx < m.params.size()) {
                            ch->setKey(m.params[pidx++]);
                        } else {
                            _s->sendToClient(fd, crlf(":ircserv 461 MODE :Not enough parameters"));
                            return;
                        }
                    } else {
                        ch->setKey("");
                    }
                }
                else if (f == 'l') { if (add) { if (pidx < m.params.size()) { int lim = std::atoi(m.params[pidx++].c_str()); if (lim < 0) lim = 0; ch->setLimit(lim); } } else { ch->setLimit(0); } }
                else if (f == 'o') { if (pidx < m.params.size()) { Client *dst = _s->findClientByNick(m.params[pidx++]); if (dst) { if (add) ch->addOp(dst->getFd()); else ch->removeOp(dst->getFd()); } }
                }
                else {
                    _s->sendToClient(fd, crlf(":ircserv 472 " + std::string(1, f) + " :Unknown mode flag"));
                    return;
                }
            }
            _s->broadcastToChannel(target, -1, ":" + c->getPrefix() + " MODE " + target + " " + flags + "\r\n");
            return;
        } else {
            _s->sendToClient(fd, crlf(":ircserv 501 :Unknown MODE flag"));
        }
        return;
    }

    if (m.command == Cmd::WHOIS) {
        if (m.params.empty()) {
            _s->sendToClient(fd, crlf(":ircserv 431 :No nickname given"));
            return;
        }
        std::string targetNick = m.params[0];
        Client *target = _s->findClientByNick(targetNick);
        if (!target) {
            _s->sendToClient(fd, crlf(":ircserv 401 " + targetNick + " :No such nick/channel"));
            return;
        }
        _s->sendToClient(fd, crlf(":ircserv 311 " + c->getNick() + " " + target->getNick() + " " + target->getUser() + " " + target->getHost() + " * :" + target->getReal()));
        _s->sendToClient(fd, crlf(":ircserv 318 " + c->getNick() + " " + target->getNick() + " :End of WHOIS list"));
        return;
    }

    // unknown command handling
    _s->sendToClient(fd, crlf(":ircserv 421 " + m.command + " :Unknown command"));
}
