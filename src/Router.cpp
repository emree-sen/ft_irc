#include "Router.hpp"
#include "Parser.hpp"
#include "Command.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <iostream>

static std::string crlf(const std::string &s) { return s + "\r\n"; }

void Router::handle(int fd, const std::string &line) {
    Message m = Parser::parse(line);
    for (size_t i = 0; i < m.command.size(); ++i) m.command[i] = std::toupper(m.command[i]);

    Client *c = _s->getClient(fd);
    if (!c) return;

    if (m.command == Cmd::PING) {
        if (m.params.empty()) { _s->sendToClient(fd, crlf(":ircserv 409 :No origin specified")); return; }
        _s->sendToClient(fd, crlf(":ircserv PONG :" + m.params[0]));
        return;
    }
    if (m.command == Cmd::PONG) return;

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

        if (newNick.empty() || newNick.length() > 9) {
            _s->sendToClient(fd, crlf(":ircserv 432 * " + newNick + " :Erroneous nickname"));
            return;
        }

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
            _s->sendToClient(fd, crlf(":ircserv 001 " + c->getNick() + " :Welcome to the ft_irc network, " + c->getNick()));
            _s->sendToClient(fd, crlf(":ircserv 002 " + c->getNick() + " :Your host is ircserv, running version 1.0"));
            _s->sendToClient(fd, crlf(":ircserv 003 " + c->getNick() + " :This server was created sometime"));
            _s->sendToClient(fd, crlf(":ircserv 004 " + c->getNick() + " ircserv 1.0 io bikol"));
            std::cout << "Client authenticated: " << c->getPrefix() << std::endl;
        }
        return;
    }

    bool isKnownCommand = (m.command == Cmd::QUIT || m.command == Cmd::JOIN ||
                          m.command == Cmd::PART || m.command == Cmd::PRIVMSG ||
                          m.command == Cmd::MODE ||
                          m.command == Cmd::TOPIC || m.command == Cmd::INVITE ||
                          m.command == Cmd::KICK);

    if (isKnownCommand && !c->authed()) {
        _s->sendToClient(fd, crlf(":ircserv 451 :You have not registered"));
        return;
    }

    if (m.command == Cmd::QUIT) {
        std::vector<std::string> chans = c->getChannels();
        std::string quitReason = m.params.empty() ? "Client Quit" : m.params[0];
        std::string quitMsg = ":" + c->getPrefix() + " QUIT :" + quitReason;

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
        if (!ch->getKey().empty()) {
            if (m.params.size() < 2 || m.params[1] != ch->getKey()) {
                _s->sendToClient(fd, crlf(":ircserv 475 " + chanName + " :Cannot join channel (+k)")); return;
            }
        }
        if (ch->isInviteOnly() && !ch->isInvited(fd)) {
            _s->sendToClient(fd, crlf(":ircserv 473 " + chanName + " :Cannot join channel (+i)")); return;
        }

        if (ch->getLimit() > 0 && (int)ch->getMembers().size() >= ch->getLimit() && !ch->isInvited(fd)) {
            _s->sendToClient(fd, crlf(":ircserv 471 " + chanName + " :Channel is full")); return;
        }
        ch->addMember(fd);
        c->joinChannel(chanName);
        if (ch->getMembers().size() == 1) ch->addOp(fd);
        std::string joinMsg = ":" + c->getPrefix() + " JOIN " + chanName;
        _s->sendToClient(fd, crlf(joinMsg));
        _s->broadcastToChannel(chanName, fd, crlf(joinMsg));
        if (!ch->getTopic().empty()) _s->sendToClient(fd, crlf(":ircserv 332 " + c->getNick() + " " + chanName + " :" + ch->getTopic()));

        std::string members;
        const std::vector<int> &fds = ch->getMembers();
        for (size_t i = 0; i < fds.size(); ++i) {
            Client *member = _s->getClient(fds[i]);
            if (member) {
                if (!members.empty()) members += " ";
                if (ch->isOp(fds[i])) members += "@";
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

        std::string partReason = (m.params.size() >= 2) ? m.params[1] : "No reason given";
        std::cout << "[SERVER] " << c->getNick() << " (" << c->getUser() << ") left channel "
                  << chanName << " (reason: " << partReason << ")" << std::endl;

        std::string partMsg = ":" + c->getPrefix() + " PART " + chanName + (m.params.size()>=2?" :"+m.params[1]:"");
        _s->broadcastToChannel(chanName, fd, crlf(partMsg));
        _s->sendToClient(fd, crlf(partMsg));
        return;
    }

    if (m.command == Cmd::PRIVMSG) {
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
        if (m.params.size() == 1) {
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

        const std::vector<int> &members = ch->getMembers();
        for (size_t i = 0; i < members.size(); ++i) {
            _s->sendToClient(members[i], crlf(msg));
        }

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
                std::string reply = ch->fullModeString();
                _s->sendToClient(fd, crlf(":ircserv 324 " + target + " " + reply));
                return;
            }
            if (!ch->isMember(fd)) {
                _s->sendToClient(fd, crlf(":ircserv 442 " + target + " :You're not on that channel"));
                return;
            }
            if (!ch->isOp(fd)) {
                _s->sendToClient(fd, crlf(":ircserv 482 " + target + " :You're not channel operator"));
                return;
            }

            bool add = true;
            size_t pidx = 2;
            std::string flagsInput = m.params[1];
            std::string applied;
            std::vector<std::string> modeParams;

            for (size_t i = 0; i < flagsInput.size(); ++i) {
                char f = flagsInput[i];
                if (f == '+') { add = true; continue; }
                if (f == '-') { add = false; continue; }
                bool changed = false;
                switch (f) {
                    case 'i':
                        if (ch->isInviteOnly() != add) { ch->setInviteOnly(add); changed = true; }
                        break;
                    case 't':
                        if (ch->isTopicRestricted() != add) { ch->setTopicRestricted(add); changed = true; }
                        break;
                    case 'k':
                        if (add) {
                            if (pidx < m.params.size()) { ch->setKey(m.params[pidx]); modeParams.push_back(m.params[pidx]); pidx++; changed = true; }
                            else { _s->sendToClient(fd, crlf(":ircserv 461 MODE :Not enough parameters")); return; }
                        } else {
                            if (!ch->getKey().empty()) { ch->setKey(""); changed = true; }
                        }
                        break;
                    case 'l':
                        if (add) {
                            if (pidx < m.params.size()) { int lim = std::atoi(m.params[pidx].c_str()); if (lim < 0) lim = 0; ch->setLimit(lim); modeParams.push_back(m.params[pidx]); pidx++; changed = true; }
                            else { _s->sendToClient(fd, crlf(":ircserv 461 MODE :Not enough parameters")); return; }
                        } else {
                            if (ch->getLimit() > 0) { ch->setLimit(0); changed = true; }
                        }
                        break;
                    case 'o':
                        if (pidx < m.params.size()) {
                            Client *dst = _s->findClientByNick(m.params[pidx]);
                            if (dst && ch->isMember(dst->getFd())) {
                                if (add) {
                                    if (!ch->isOp(dst->getFd())) { ch->addOp(dst->getFd()); changed = true; }
                                } else {
                                    if (ch->isOp(dst->getFd())) { ch->removeOp(dst->getFd()); changed = true; }
                                }
                                modeParams.push_back(m.params[pidx]);
                            }
                            pidx++;
                        } else {
                            _s->sendToClient(fd, crlf(":ircserv 461 MODE :Not enough parameters"));
                            return;
                        }
                        break;
                    default:
                        _s->sendToClient(fd, crlf(":ircserv 472 " + std::string(1, f) + " :Unknown mode flag"));
                        return;
                }
                if (changed) {
                    applied += (add ? "+" : "-");
                    applied += f;
                }
            }
            if (!applied.empty()) {
                std::string paramStr;
                for (size_t i = 0; i < modeParams.size(); ++i) {
                    if (i) paramStr += ' ';
                    paramStr += modeParams[i];
                }
                std::string out = ":" + c->getPrefix() + " MODE " + target + " " + applied;
                if (!paramStr.empty()) out += " " + paramStr;
                out += "\r\n";
                _s->broadcastToChannel(target, -1, out);
            }
            return;
        } else {
            _s->sendToClient(fd, crlf(":ircserv 501 :Unknown MODE flag"));
        }
        return;
    }

    _s->sendToClient(fd, crlf(":ircserv 421 " + m.command + " :Unknown command"));
}
