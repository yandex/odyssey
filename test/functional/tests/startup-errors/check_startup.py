import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest


ODYSSEY_BIN = sys.argv.pop(1) if len(sys.argv) > 1 else '/usr/bin/odyssey'


def startup(parameters):
    body = struct.pack('!I', 196608) + parameters
    return struct.pack('!I', len(body) + 4) + body


def with_options(options):
    return startup(b'user\0test\0database\0console\0options\0' + options + b'\0\0')


def read_exact(conn, size):
    result = b''
    while len(result) < size:
        chunk = conn.recv(size - len(result))
        if not chunk:
            raise AssertionError('connection closed before a complete response')
        result += chunk
    return result


def read_message(conn):
    header = read_exact(conn, 5)
    length = struct.unpack('!I', header[1:])[0]
    if not 4 <= length <= 65536:
        raise AssertionError(f'invalid response length: {length}')
    return header[:1], read_exact(conn, length - 4)


class StartupErrorsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temp = tempfile.TemporaryDirectory(prefix='odyssey-startup-errors-')
        cls.addClassCleanup(temp.cleanup)
        root = pathlib.Path(temp.name)
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            cls.port = listener.getsockname()[1]
        config = root / 'odyssey.conf'
        config.write_text(f'''
daemonize no
workers 1
resolvers 1
log_to_stdout yes
log_format "%l %m\\n"
log_session no
log_stats no
locks_dir "{root}"
listen {{ host "127.0.0.1" port {cls.port} }}
storage "local" {{ type "local" }}
database "console" {{
    user default {{ authentication "none" pool "session" storage "local" }}
}}
''')
        log = tempfile.TemporaryFile(mode='w+')
        cls.addClassCleanup(log.close)
        proc = subprocess.Popen([ODYSSEY_BIN, str(config)], stdout=log, stderr=log)

        def stop():
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

        cls.addClassCleanup(stop)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                break
            try:
                with socket.create_connection(('127.0.0.1', cls.port), timeout=0.1):
                    return
            except OSError:
                time.sleep(0.02)
        log.seek(0)
        raise AssertionError('Odyssey failed to start:\n' + log.read())

    def test_invalid_startup_parameters_receive_protocol_error(self):
        cases = {
            'oversized value': with_options(b'-c search_path=' + b'a' * 128),
            'oversized name': with_options(b'--' + b'a' * 128 + b'=x'),
            'missing equals': with_options(b'-c search_path'),
            'missing assignment': with_options(b'-c'),
            'invalid switch': with_options(b'-x search_path=public'),
            'missing user': startup(b'database\0console\0\0'),
            'unterminated value': startup(b'user\0test'),
            'missing list terminator': startup(b'user\0test\0'),
        }
        for name, packet in cases.items():
            with self.subTest(name=name), socket.create_connection(
                ('127.0.0.1', self.port), timeout=3
            ) as conn:
                conn.sendall(packet)
                kind, body = read_message(conn)
                self.assertEqual(kind, b'E')
                fields = {field[:1]: field[1:] for field in body.split(b'\0') if field}
                self.assertEqual(fields[b'S'], b'FATAL')
                self.assertEqual(fields[b'C'], b'08P01')
                self.assertIn(b'invalid startup packet parameters', fields[b'M'])
                self.assertIn(b'127 bytes', fields[b'M'])

    def test_options_at_the_limit_are_accepted(self):
        for options in (b'-c search_path=' + b'a' * 127, b'--' + b'a' * 127 + b'=x'):
            with self.subTest(options=options), socket.create_connection(
                ('127.0.0.1', self.port), timeout=3
            ) as conn:
                conn.sendall(with_options(options))
                while True:
                    kind, body = read_message(conn)
                    self.assertNotEqual(kind, b'E', body)
                    if kind == b'Z':
                        break
                conn.sendall(b'X\0\0\0\4')


if __name__ == '__main__':
    unittest.main()
