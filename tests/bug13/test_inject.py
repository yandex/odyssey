#!/usr/bin/env python3
"""Deterministic repro of the global-pstmt bug-13 (heap-use-after-free
/ od_assert(klock.kvp != NULL) in od_global_pstmt_try_remove).

Requires odyssey built from this branch (ASAN recommended) and a
console-reachable instance with two independent routes, both with
pool_reserve_prepared_statement yes (see odyssey.conf in this dir).

Env:
    OD_HOST  - odyssey host           (default 127.0.0.1)
    OD_PORT  - odyssey port           (default 17102)
    OD_LOG   - odyssey log file, used to observe injection parkings
               (required: parking detection is the sync point)

Scenario (see README.md):
  A(pg): Parse s1+Sync; RST        -> holders: map + serverA
  arm, DROP SERVERS                -> unref serverA: v=2 -> PARK #1
  B(simplex): Parse s1+Sync; RST   -> holders: map + serverB
  DROP SERVERS                     -> unref serverB: v=2 -> PARK #2
  release -> both racers wakeup, winner removes+frees the entry,
  loser hashes the freed key -> heap-use-after-free (or va == NULL assert)
"""
import os
import socket
import struct
import sys
import time

HOST = os.environ.get("OD_HOST", "127.0.0.1")
PORT = int(os.environ.get("OD_PORT", "17102"))
SRVLOG = os.environ.get("OD_LOG", "")

RFQ = b"Z\x00\x00\x00\x05I"


def cstr(s):
    if isinstance(s, str):
        s = s.encode()
    return s + b"\x00"


def msg(t, data):
    return t + struct.pack(">i", len(data) + 4) + data


def startup(db=b"postgres"):
    body = struct.pack(">i", 196608)
    for k, v in {"user": b"postgres", "database": db}.items():
        body += cstr(k) + cstr(v)
    body += b"\x00"
    return struct.pack(">i", len(body) + 4) + body


def recv_until_rfq(sock, timeout=20):
    sock.settimeout(timeout)
    data = b""
    end = time.time() + timeout
    while time.time() < end and not data.endswith(RFQ):
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
    return data


def connect(db=b"postgres"):
    s = socket.create_connection((HOST, PORT), timeout=10)
    s.sendall(startup(db=db))
    r = recv_until_rfq(s)
    assert r.endswith(RFQ), "bad startup: %r" % r[-40:]
    return s


def parse_msg(name, query):
    return msg(b"P", cstr(name) + cstr(query) + struct.pack(">h", 0))


def query_msg(q):
    return msg(b"Q", cstr(q))


sync = msg(b"S", b"")


def rst(sock):
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                    struct.pack("ii", 1, 0))
    sock.close()


def wait_parkings(n, timeout=20):
    """server kills are async, so watch the log instead of sleeping"""
    if not SRVLOG:
        print("FAIL: OD_LOG is not set")
        sys.exit(1)
    end = time.time() + timeout
    while time.time() < end:
        with open(SRVLOG, "r") as f:
            if f.read().count("injection pstmt_try_remove: parking") >= n:
                return True
        time.sleep(0.2)
    return False


def od_alive():
    try:
        s = socket.create_connection((HOST, PORT), timeout=0.5)
        s.close()
        return True
    except OSError:
        return False


print("== A(postgres): Parse s1 + Sync, then RST A")
a = connect(b"postgres")
a.sendall(parse_msg("s1", "select 1") + sync)
r = recv_until_rfq(a)
assert b"\x31" in r, r
rst(a)

print("== console: arm injection point")
c = connect(b"console")
c.sendall(query_msg("LOAD pstmt_try_remove"))
r = recv_until_rfq(c)
assert b"LOAD" in r and b"42601" not in r, r

print("== console: DROP SERVERS -> unref serverA (v=2) -> PARK #1")
c.sendall(query_msg("DROP SERVERS"))
if not wait_parkings(1):
    print("FAIL: park #1 never happened")
    sys.exit(1)

print("== B(simplex): Parse s1 + Sync, then RST B")
b = connect(b"simplex")
b.sendall(parse_msg("s1", "select 1") + sync)
r = recv_until_rfq(b)
assert b"\x31" in r, r
rst(b)

print("== console: DROP SERVERS -> unref serverB (v=2) -> PARK #2")
c.sendall(query_msg("DROP SERVERS"))
if not wait_parkings(2):
    print("FAIL: park #2 never happened")
    sys.exit(1)

print("== console: release -> both race for the bucket lock")
c.sendall(query_msg("LOAD pstmt_try_remove"))
time.sleep(3)

if od_alive():
    print("NO CRASH: odyssey alive")
    sys.exit(1)
print("CRASH REPRODUCED: odyssey dead")
sys.exit(0)
