import os
import pathlib
import signal
import socket
import struct
import sys
import threading


def read_exact(conn, size):
    data = b""
    while len(data) < size:
        part = conn.recv(size - len(data))
        if not part:
            raise EOFError("connection closed before ReadyForQuery")
        data += part
    return data


def read_ready(conn):
    while True:
        kind = read_exact(conn, 1)
        size = struct.unpack("!I", read_exact(conn, 4))[0]
        body = read_exact(conn, size - 4)
        if kind == b"E":
            raise RuntimeError(body)
        if kind == b"Z":
            return body


def query_message(sql):
    body = sql.encode() + b"\0"
    return b"Q" + struct.pack("!I", len(body) + 4) + body


def connect(lock_id):
    conn = socket.socket()
    conn.settimeout(2)
    conn.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
    if hasattr(socket, "TCP_WINDOW_CLAMP"):
        conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_WINDOW_CLAMP, 1024)
    conn.connect((os.environ.get("ODYSSEY_HOST", "127.0.0.1"),
                  int(os.environ.get("ODYSSEY_PORT", "6432"))))
    user = os.environ.get("ODYSSEY_USER", "postgres").encode()
    database = os.environ["PGDATABASE"].encode()
    body = b"user\0" + user + b"\0database\0" + database + b"\0\0"
    conn.sendall(struct.pack("!II", len(body) + 8, 196608) + body)
    read_ready(conn)
    conn.sendall(query_message(f"BEGIN; SELECT pg_advisory_xact_lock({lock_id})"))
    assert read_ready(conn) == b"T"
    return conn


def stall(conn, trigger, sql):
    while not stop.wait(0.02):
        if trigger.exists():
            try:
                conn.sendall(query_message(sql) * 400000)
            except OSError:
                pass
            return


work = pathlib.Path(sys.argv[1])
stop = threading.Event()
signal.signal(signal.SIGTERM, lambda *_: stop.set())
clients = []
try:
    for name, lock_id, sql in (
        ("set", 759103, "SET odyssey.opportunistic_acquire = off"),
        ("error", 759104, "SET odyssey.opportunistic_acquire = 'invalid'"),
    ):
        conn = connect(lock_id)
        clients.append(conn)
        threading.Thread(target=stall, args=(conn, work / f"{name}.go", sql),
                         daemon=True).start()
    (work / "ready").touch()
    # Retain both sockets even if sendall times out. The isolation test must
    # observe the transaction being released by Odyssey, not by this fixture.
    stop.wait()
finally:
    for conn in clients:
        conn.close()
