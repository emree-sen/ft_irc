# ft_irc — IRC server (C++98)

Build and run:

```
make
./ircserv <port> <password>
```

Connect using netcat or any IRC client (irssi, weechat):

```
nc -C 127.0.0.1 6667
PASS secret
NICK alice
USER alice 0 * :Alice
JOIN #test
PRIVMSG #test :hello
```

Türkçe ayrıntılı kullanım kılavuzu (KVIrc dahil): USAGE.md

Features:
- Single poll() loop, all sockets non-blocking
- Multi-client, IPv6 (works with IPv4 via v6-mapped)
- PASS/NICK/USER, PING/PONG, QUIT
- JOIN/PART/PRIVMSG/NOTICE
- Channels with ops and modes: i, t, k, o, l
- Simple numeric replies

Notes:
- No external libraries; C++98; compiled with -Wall -Wextra -Werror
- This is a minimal educational IRC server, not production ready.
# ft_irc
