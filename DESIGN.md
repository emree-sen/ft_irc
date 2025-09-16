# DESIGN

High-level architecture:
- Server: socket setup, accept, poll loop, dispatch
- Poller: tiny wrapper over poll()
- Client: per-connection state (auth, buffers, channels)
- Channel: membership, ops, modes (i,t,k,o,l), topic, invites
- Parser: IRC line parsing (prefix, command, params)
- Router: command handlers

Non-blocking I/O:
- Listening and client sockets set O_NONBLOCK
- Single poll() call per loop; accept/read/write handled via readiness
- Read buffer accumulates bytes; split by CRLF; partial reads supported
- Write queue per client; partial send supported

Authentication:
- Sequence: PASS (mandatory), then NICK and USER; once complete, 001 sent
- Unauthenticated clients cannot use channel/message commands

Channels and modes:
- First member becomes operator
- Modes: +i/+t/+k <key>/+o <nick>/+l <n> with corresponding behavior

Error handling:
- Graceful removal on errors/EOF; no blocking calls

Threading:
- Single-threaded; single poll loop
