#!/usr/bin/env python3
"""
Evil TCP proxy: forwards normal PG connections transparently,
but hangs cancel request connections (never closes the client socket).

This simulates a network condition where the backend processes
the cancel but the TCP connection never delivers EOF to Odyssey.

Usage: evil_proxy.py <listen_port> <pg_host> <pg_port>
"""

import socket
import struct
import sys
import threading
import os
import signal

cancel_count = 0
cancel_lock = threading.Lock()


def forward(src, dst, label):
    try:
        while True:
            data = src.recv(65536)
            if not data:
                break
            dst.sendall(data)
    except Exception:
        pass
    try:
        dst.shutdown(socket.SHUT_WR)
    except Exception:
        pass


def handle_client(client_sock, pg_host, pg_port):
    global cancel_count
    try:
        # Peek at the first 8 bytes to detect cancel request
        header = b''
        while len(header) < 8:
            chunk = client_sock.recv(8 - len(header), socket.MSG_PEEK)
            if not chunk:
                client_sock.close()
                return
            header += chunk

        msg_len = struct.unpack('!I', header[0:4])[0]
        code = struct.unpack('!I', header[4:8])[0]

        is_cancel = (msg_len == 16 and code == 80877102)

        # Connect to real PG
        pg_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        pg_sock.connect((pg_host, pg_port))

        if is_cancel:
            # Read the full 16-byte cancel message and forward it
            cancel_msg = client_sock.recv(16)
            pg_sock.sendall(cancel_msg)
            pg_sock.close()

            with cancel_lock:
                cancel_count += 1
                n = cancel_count
            print(f"[PROXY] cancel #{n}: forwarded to PG, now hanging client socket...",
                  flush=True)
            # Hang: keep client_sock open forever, never close it.
            # This simulates the condition where Odyssey's od_read()
            # never gets EOF.
            event = threading.Event()
            event.wait()  # blocks forever
        else:
            # Normal connection: bidirectional proxy
            t1 = threading.Thread(target=forward, args=(client_sock, pg_sock, "c->s"), daemon=True)
            t2 = threading.Thread(target=forward, args=(pg_sock, client_sock, "s->c"), daemon=True)
            t1.start()
            t2.start()
            t1.join()
            t2.join()
            client_sock.close()
            pg_sock.close()

    except Exception as e:
        print(f"[PROXY] error: {e}", flush=True)
        try:
            client_sock.close()
        except Exception:
            pass


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <listen_port> <pg_host> <pg_port>", file=sys.stderr)
        sys.exit(1)

    listen_port = int(sys.argv[1])
    pg_host = sys.argv[2]
    pg_port = int(sys.argv[3])

    # Write PID file
    with open('/tmp/evil_proxy.pid', 'w') as f:
        f.write(str(os.getpid()))

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', listen_port))
    server.listen(256)

    print(f"[PROXY] evil proxy pid={os.getpid()} listening on 127.0.0.1:{listen_port}, "
          f"forwarding to {pg_host}:{pg_port}", flush=True)

    while True:
        client_sock, addr = server.accept()
        t = threading.Thread(target=handle_client, args=(client_sock, pg_host, pg_port), daemon=True)
        t.start()


if __name__ == '__main__':
    main()
