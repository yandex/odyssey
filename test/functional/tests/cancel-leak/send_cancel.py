#!/usr/bin/env python3
"""
Fire N parallel cancel requests against Odyssey.

Each cancel: connect, start pg_sleep, send CancelRequest, close.
All N cancels are launched in parallel threads.

Usage: send_cancel.py <host> <port> <user> <dbname> [count]
"""

import socket
import struct
import sys
import time
import threading


def _recv_exact(sock, n):
    buf = b''
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("connection closed")
        buf += chunk
    return buf


def pg_connect(host, port, user, dbname):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect((host, port))
    params = f"user\x00{user}\x00database\x00{dbname}\x00\x00"
    msg = struct.pack('!II', 4 + 4 + len(params), 0x00030000) + params.encode()
    sock.sendall(msg)
    pid, secret = None, None
    while True:
        hdr = _recv_exact(sock, 5)
        msg_type = hdr[0:1]
        msg_len = struct.unpack('!I', hdr[1:5])[0]
        body = _recv_exact(sock, msg_len - 4)
        if msg_type == b'E':
            raise RuntimeError(f"PG error: {body.decode('utf-8', errors='replace')}")
        if msg_type == b'R':
            auth_type = struct.unpack('!I', body[:4])[0]
            if auth_type != 0:
                raise RuntimeError(f"Unsupported auth type: {auth_type}")
        if msg_type == b'K':
            pid, secret = struct.unpack('!II', body[:8])
        if msg_type == b'Z':
            return sock, pid, secret


def send_query(sock, query):
    q = query.encode() + b'\x00'
    msg = b'Q' + struct.pack('!I', 4 + len(q)) + q
    sock.sendall(msg)


def send_cancel_packet(host, port, pid, secret):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect((host, port))
    msg = struct.pack('!IIII', 16, 80877102, pid, secret)
    sock.sendall(msg)
    time.sleep(0.5)
    sock.close()


def do_one_cancel(idx, host, port, user, dbname, results):
    try:
        sock, pid, secret = pg_connect(host, port, user, dbname)
        send_query(sock, "SELECT pg_sleep(600)")
        time.sleep(0.5)
        send_cancel_packet(host, port, pid, secret)
        # Read cancel response
        try:
            sock.settimeout(10)
            while True:
                hdr = _recv_exact(sock, 5)
                msg_type = hdr[0:1]
                msg_len = struct.unpack('!I', hdr[1:5])[0]
                body = _recv_exact(sock, msg_len - 4)
                if msg_type == b'Z':
                    break
        except Exception:
            pass
        sock.close()
        results[idx] = "OK"
        print(f"  cancel #{idx+1}: OK", flush=True)
    except Exception as e:
        results[idx] = f"FAIL: {e}"
        print(f"  cancel #{idx+1}: FAIL: {e}", flush=True)


def main():
    if len(sys.argv) < 5:
        print(f"Usage: {sys.argv[0]} <host> <port> <user> <dbname> [count]", file=sys.stderr)
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2])
    user = sys.argv[3]
    dbname = sys.argv[4]
    count = int(sys.argv[5]) if len(sys.argv) > 5 else 1

    print(f"Firing {count} parallel cancel requests...", flush=True)
    results = [None] * count
    threads = []
    for i in range(count):
        t = threading.Thread(target=do_one_cancel, args=(i, host, port, user, dbname, results))
        threads.append(t)

    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=30)

    ok = sum(1 for r in results if r == "OK")
    fail = sum(1 for r in results if r and r.startswith("FAIL"))
    print(f"Done: {ok} OK, {fail} FAIL out of {count}", flush=True)


if __name__ == '__main__':
    main()
