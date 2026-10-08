#!/usr/bin/env python3
"""Protocol and shutdown checks against a local trust-auth PostgreSQL backend.

Usage: python3 test/tls_workers/test_handoff.py build/sources/odyssey \
           --postgres-port 15432 --output /tmp/odyssey-handoff-test
Uses only the Python standard library and openssl; starts/stops its own Odyssey.
"""
import argparse
import concurrent.futures
import contextlib
import pathlib
import signal
import socket
import ssl
import struct
import subprocess
import time

SSL_REQUEST = struct.pack('!II', 8, 80877103)
GSS_REQUEST = struct.pack('!II', 8, 80877104)
CANCEL_REQUEST = struct.pack('!IIII', 16, 80877102, 123456789, 987654321)
BODY = struct.pack('!I', 196608) + b'user\0postgres\0database\0postgres\0\0'
STARTUP = struct.pack('!I', len(BODY) + 4) + BODY
QUERY = b'Q' + struct.pack('!I', len(b'select 67;\0') + 4) + b'select 67;\0'


def exact(sock, size):
    data = b''
    while len(data) < size:
        part = sock.recv(size - len(data))
        if not part:
            raise EOFError(f'EOF after {len(data)}/{size} bytes')
        data += part
    return data


def packet(sock):
    kind = exact(sock, 1)
    size, = struct.unpack('!I', exact(sock, 4))
    assert 4 <= size <= 1024 * 1024
    return kind, exact(sock, size - 4)


def closed(sock):
    try:
        assert sock.recv(1) == b''
    except (ConnectionResetError, ssl.SSLEOFError):
        pass


def secure(sock):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    return ctx.wrap_socket(sock, server_hostname='localhost')


def query(sock):
    # One write also exercises readahead ownership for pipelined plain startup.
    sock.sendall(STARTUP + QUERY)
    ready = 0
    row = False
    while ready != 2:
        kind, data = packet(sock)
        assert kind != b'E', data
        if kind == b'D':
            assert data == struct.pack('!HI', 1, 2) + b'67', data
            row = True
        ready += kind == b'Z'
    assert row
    sock.sendall(b'X\0\0\0\4')


class Odyssey:
    def __init__(self, args, mode, tls_workers):
        self.args = args
        self.path = args.output / f'{mode}-{tls_workers}'
        self.path.mkdir(parents=True, exist_ok=True)
        self.proc = None
        self.log = None
        self.port = args.port
        (self.path / 'odyssey.conf').write_text(f'''
daemonize no
log_to_stdout no
log_file "{self.path}/odyssey.log"
log_format "%p %t %l [%i %s] (%c) %m\\n"
log_session no
log_config no
log_stats no
locks_dir "{self.path}"
unix_socket_dir "{self.path}"
unix_socket_mode "0700"
workers 1
tls_workers {tls_workers}
client_max_routing 128
listen {{
    host "127.0.0.1"
    port {self.port}
    tls "{mode}"
    tls_key_file "{args.output}/server.key"
    tls_cert_file "{args.output}/server.pem"
    client_login_timeout 1500
}}
storage "pg" {{
    type "remote"
    host "127.0.0.1"
    port {args.postgres_port}
}}
database default {{
    user default {{
        authentication "none"
        storage "pg"
        pool "session"
        pool_size 64
    }}
}}
''')

    def raw(self):
        return socket.create_connection(('127.0.0.1', self.port), timeout=4)

    def connect(self, tls=False, gss=False):
        sock = self.raw()
        try:
            if gss:
                sock.sendall(GSS_REQUEST)
                assert exact(sock, 1) == b'N'
            if tls:
                sock.sendall(SSL_REQUEST)
                assert exact(sock, 1) == b'S'
                sock = secure(sock)
            return sock
        except BaseException:
            sock.close()
            raise

    def __enter__(self):
        self.log = (self.path / 'process.log').open('w')
        self.proc = subprocess.Popen([str(self.args.odyssey), str(self.path / 'odyssey.conf')],
                                     stdout=self.log, stderr=subprocess.STDOUT)
        try:
            for _ in range(100):
                assert self.proc.poll() is None, (self.path / 'process.log').read_text()
                try:
                    self.raw().close()
                    return self
                except ConnectionRefusedError:
                    time.sleep(.02)
            raise RuntimeError('Odyssey did not listen')
        except BaseException:
            if self.proc.poll() is None:
                self.proc.kill()
            self.proc.wait()
            self.log.close()
            raise

    def __exit__(self, *unused):
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGTERM)
            try:
                self.proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
                raise AssertionError('Odyssey shutdown hung')
        self.log.close()
        assert self.proc.returncode == 0, (self.path / 'process.log').read_text()


def check_allow(args, workers):
    with Odyssey(args, 'allow', workers) as od:
        for tls in (False, True):
            for gss in (False, True):
                with od.connect(tls, gss) as sock:
                    query(sock)
                with od.connect(tls, gss) as sock:
                    sock.sendall(CANCEL_REQUEST)
                    closed(sock)
        for repeat in (SSL_REQUEST, GSS_REQUEST):
            with od.connect(True, repeat == GSS_REQUEST) as sock:
                sock.sendall(repeat)
                closed(sock)
        with od.connect(True) as sock:
            sock.sendall(GSS_REQUEST)
            assert exact(sock, 1) == b'N'
            query(sock)
        with od.raw() as sock:
            sock.sendall(SSL_REQUEST + b'garbage')
            assert exact(sock, 1) == b'S'
            closed(sock)
        for _ in range(8):
            with od.raw() as sock:
                sock.sendall(SSL_REQUEST)
                assert exact(sock, 1) == b'S'
        with contextlib.ExitStack() as stack:
            slow = [stack.enter_context(od.raw()) for _ in range(8)]
            for sock in slow[:4]:
                sock.sendall(SSL_REQUEST)
                assert exact(sock, 1) == b'S'
            with od.connect(True) as sock:
                query(sock)
            for sock in slow:
                closed(sock)
        # TLS workers must drain in-flight handshakes before normal workers stop.
        with contextlib.ExitStack() as stack:
            pending = [stack.enter_context(od.raw()) for _ in range(32)]
            for sock in pending:
                sock.sendall(SSL_REQUEST)
                assert exact(sock, 1) == b'S'
            od.proc.send_signal(signal.SIGTERM)
            time.sleep(.05)
            def finish(sock):
                with secure(sock) as encrypted:
                    query(encrypted)
            with concurrent.futures.ThreadPoolExecutor(max_workers=16) as pool:
                list(pool.map(finish, pending))
            assert od.proc.wait(timeout=8) == 0
    print(f'allow, tls_workers={workers}: protocol, buffering, timeouts, aborts, shutdown OK', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('odyssey', type=pathlib.Path)
    parser.add_argument('--postgres-port', type=int, default=15432)
    parser.add_argument('--port', type=int, default=16432)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.odyssey = args.odyssey.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
                    '-subj', '/CN=localhost', '-keyout', str(args.output / 'server.key'),
                    '-out', str(args.output / 'server.pem')], check=True, capture_output=True)
    for workers in (0, 1, 2):
        check_allow(args, workers)
    with Odyssey(args, 'require', 1) as od:
        with od.raw() as sock:
            sock.sendall(STARTUP)
            kind, body = packet(sock)
            assert kind == b'E' and b'SSL is required' in body, (kind, body)
        with od.connect(True) as sock:
            query(sock)
    print('require: plaintext rejected, TLS query OK', flush=True)
    with Odyssey(args, 'disable', 1) as od:
        with od.raw() as sock:
            sock.sendall(SSL_REQUEST)
            assert exact(sock, 1) == b'N'
            query(sock)
    print('disable: plaintext bypass with TLS workers configured OK', flush=True)


if __name__ == '__main__':
    main()
