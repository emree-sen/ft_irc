#!/usr/bin/env python3
import socket, time, sys

HOST='127.0.0.1'
PORT=6667
PASS='pass'

def recvall(sock, timeout=0.2):
    sock.setblocking(False)
    data=b''
    end=time.time()+timeout
    while time.time()<end:
        try:
            chunk=sock.recv(4096)
            if not chunk: break
            data+=chunk
        except BlockingIOError:
            time.sleep(0.01)
    return data.decode('utf-8','ignore')

def send(s, line):
    if not line.endswith('\r\n'): line+='\r\n'
    s.sendall(line.encode())

# Basic integration flow
sA=socket.socket(); sA.connect((HOST,PORT))
sB=socket.socket(); sB.connect((HOST,PORT))

# register A
send(sA, f"PASS {PASS}")
send(sA, "NICK Alice")
send(sA, "USER a a a :Alice")
# register B
send(sB, f"PASS {PASS}")
send(sB, "NICK Bob")
send(sB, "USER b b b :Bob")

# consume welcomes
time.sleep(0.2)
recvall(sA); recvall(sB)

# Alice creates channel
send(sA, "JOIN #chan")
# Bob joins
send(sB, "JOIN #chan")

# Bob tries MODE +t (should fail 482)
send(sB, "MODE #chan +t")
# Alice sets +t
send(sA, "MODE #chan +t")
# Bob tries to set topic (should fail 482)
send(sB, "TOPIC #chan :Test Topic")
# Alice gives op to Bob
send(sA, "MODE #chan +o Bob")
# Bob sets topic (should succeed)
send(sB, "TOPIC #chan :New Topic")
# Alice removes op from Bob
send(sA, "MODE #chan -o Bob")
# Bob tries topic again (fail)
send(sB, "TOPIC #chan :Another Topic")

# gather output
time.sleep(0.5)
outA=recvall(sA,0.5)
outB=recvall(sB,0.5)
print('--- Alice ---')
print(outA)
print('--- Bob ---')
print(outB)

# very light asserts
assert ' 482 ' in outB, 'Bob should receive 482 when changing mode or topic initially'
assert ' MODE #chan +t' in outA or ' MODE #chan +t' in outB, 'Mode change +t not broadcast'
assert ' MODE #chan +o Bob' in outA or ' MODE #chan +o Bob' in outB, '+o not applied'
assert ' MODE #chan -o Bob' in outA or ' MODE #chan -o Bob' in outB, '-o not applied'

print('OK')
