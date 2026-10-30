import socket
import struct
import sys
import time

# open connections, request TLS and never send ClientHello
count = int(sys.argv[1])
duration = float(sys.argv[2])

conns = []
for _ in range(count):
    s = socket.create_connection(("127.0.0.1", 6432))
    s.sendall(struct.pack("!ii", 8, 80877103))
    assert s.recv(1) == b"S"
    conns.append(s)

time.sleep(duration)
