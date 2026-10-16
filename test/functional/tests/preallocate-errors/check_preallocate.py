#!/usr/bin/env python3
"""Run against a Debug/ASAN binary to check preallocation cleanup asserts."""

import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import unittest


ODYSSEY = os.environ.get("ODYSSEY_BIN", "/usr/bin/odyssey")


class PreallocateErrors(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ody-prealloc-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.backend = socket.socket(socket.AF_UNIX)
        self.addCleanup(self.backend.close)
        self.backend.bind(str(self.root / "backend.sock"))
        self.backend.settimeout(10)

    def start_odyssey(self, tls):
        config = self.root / "odyssey.conf"
        config.write_text(f'''
daemonize no
workers 1
resolvers 1
locks_dir "{self.root}"
unix_socket_dir "{self.root}"
unix_socket_mode "0700"
log_to_stdout yes
log_format "%l (%c) %m\\n"
log_session yes
log_stats no
listen {{
    port 6432
}}
storage "backend" {{
    type "remote"
    host "unix://{self.root}/backend.sock"
    tls "{tls}"
}}
database "postgres" {{
    user "postgres" {{
        authentication "none"
        storage "backend"
        pool "session"
        min_pool_size 1
        pool_size 5
    }}
}}
''')
        self.log_path = self.root / "odyssey.log"
        output = self.log_path.open("w")
        self.addCleanup(output.close)
        self.process = subprocess.Popen([ODYSSEY, str(config)], stdout=output,
                                        stderr=subprocess.STDOUT)
        self.addCleanup(self.stop_odyssey)

    def stop_odyssey(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()

    def assert_alive(self):
        self.assertIsNone(self.process.poll(), self.log_path.read_text())

    def wait_for_log(self, message, count=1):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            self.assert_alive()
            if self.log_path.read_text().count(message) >= count:
                return
            time.sleep(0.05)
        self.fail(f"Timed out waiting for {message!r}:\n"
                  f"{self.log_path.read_text()}")

    def check_refused_connect(self, tls):
        # A bound Unix socket that is not listening refuses connect reliably.
        self.start_odyssey(tls)
        self.wait_for_log("failed to connect to", count=2)
        # The same backend becomes available: preallocation must recover.
        self.backend.listen(1)
        with self.backend.accept()[0] as conn:
            conn.settimeout(10)
            if tls == "prefer":
                self.assertEqual(conn.recv(8, socket.MSG_WAITALL),
                                 struct.pack("!II", 8, 80877103))
                conn.sendall(b"N")
            self.wait_for_log("idle server connection preallocated")
            self.assert_alive()

    def test_refused_connect_without_tls(self):
        self.check_refused_connect("disable")

    def test_refused_connect_with_tls(self):
        self.check_refused_connect("prefer")

    def test_rejected_tls(self):
        self.backend.listen(1)
        self.start_odyssey("require")
        for attempt in range(2):
            with self.backend.accept()[0] as conn:
                conn.settimeout(10)
                self.assertEqual(conn.recv(8, socket.MSG_WAITALL),
                                 struct.pack("!II", 8, 80877103))
                conn.sendall(b"N")
                # No StartupMessage was sent, so cleanup must close directly,
                # without sending a PostgreSQL Terminate message.
                self.assertEqual(conn.recv(1), b"")
            self.wait_for_log("not supported, closing", count=attempt + 1)
            self.assert_alive()


if __name__ == "__main__":
    unittest.main()
