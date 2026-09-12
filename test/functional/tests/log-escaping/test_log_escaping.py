import socket
import struct
import time
from pathlib import Path


def connect():
    deadline = time.monotonic() + 5
    while True:
        try:
            return socket.create_connection(("127.0.0.1", 6432), timeout=5)
        except ConnectionRefusedError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.05)


def read(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        assert chunk, "Connection closed before ErrorResponse"
        data += chunk
    return data


def main():
    user = b"\nuser\t\\n"
    database = b"db\r=one"
    application = b"app\n\t\\name"
    params = b"\0".join((
        b"user", user, b"database", database,
        b"application_name", application, b"", b"",
    ))
    with connect() as sock:
        sock.sendall(struct.pack("!II", len(params) + 8, 196608) + params)
        assert read(sock, 1) == b"E", "Expected routing error"
        size = struct.unpack("!I", read(sock, 4))[0]
        error = read(sock, size - 4)
        assert b"C3D000\0" in error, error
        assert (b"route for '" + database + b"." + user
                + b"' is not found") in error, error

    expected = (
        b"tskv\tuser=\\nuser\\t\\\\n\tdb=db\\r\\=one"
        b"\tapp=app\\n\\t\\\\name\tctx=startup"
        b"\tmsg=route for 'db\\r\\=one.\\nuser\\t\\\\n' is not found for '"
    )
    deadline = time.monotonic() + 5
    while True:
        data = Path("/var/log/odyssey.log").read_bytes()
        lines = data.split(b"\n")
        if any(line.startswith(expected) for line in lines):
            break
        assert time.monotonic() < deadline, data
        time.sleep(0.05)

    assert all(line.startswith(b"tskv\t") for line in lines if line), data
    assert b"\r" not in data, data


if __name__ == "__main__":
    main()
