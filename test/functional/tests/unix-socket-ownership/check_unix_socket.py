#!/usr/bin/env python3
"""Exercise Unix listener ownership using real Odyssey processes."""

import errno
import os
from pathlib import Path
import signal
import socket
import stat
import struct
import subprocess
import sys
import tempfile
import time
import unittest


ODYSSEY = os.environ.get("ODYSSEY_BIN", "/usr/bin/odyssey")
CONFIG = Path(__file__).with_name("config.conf.in").read_text()


class UnixSocketOwnership(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="ody-unix-")
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.socket_path = self.root / ".s.PGSQL.6432"
        self.instances = []

    def start(self):
        directory = self.root / f"instance-{len(self.instances)}"
        directory.mkdir()
        config = directory / "odyssey.conf"
        config.write_text(CONFIG.replace("@LOCKS_DIR@", str(directory))
                          .replace("@SOCKET_DIR@", str(self.root)))
        log_path = directory / "odyssey.log"
        with log_path.open("w") as output:
            process = subprocess.Popen(
                [ODYSSEY, str(config)], stdout=output,
                stderr=subprocess.STDOUT, env={**os.environ, "LC_ALL": "C"})
        self.instances.append((process, log_path))
        self.addCleanup(self.cleanup_process, process)
        return process, log_path

    @staticmethod
    def cleanup_process(process):
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

    def connect(self):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(0.5)
            client.connect(str(self.socket_path))
            # SSLRequest proves that the listener serves the PostgreSQL
            # protocol, rather than merely having a socket path/backlog.
            client.sendall(struct.pack("!II", 8, 80877103))
            self.assertEqual(client.recv(1), b"N")

    def wait_ready(self, instance):
        process, log_path = instance
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            self.assertIsNone(process.poll(), log_path.read_text())
            try:
                self.connect()
                return
            except OSError:
                time.sleep(0.05)
        self.fail(f"Listener did not become ready:\n{log_path.read_text()}")

    def stop(self, instance):
        process, log_path = instance
        process.terminate()
        self.assertEqual(process.wait(timeout=10), 0, log_path.read_text())

    def assert_start_failed(self, instance):
        process, log_path = instance
        # A startup error must exit normally, not abort or time out.
        self.assertEqual(process.wait(timeout=10), 1, log_path.read_text())
        return log_path.read_text()

    def identity(self):
        info = self.socket_path.lstat()
        return info.st_dev, info.st_ino

    @unittest.skipUnless(sys.platform.startswith("linux"),
                         "Automatic stale socket recovery is Linux-only")
    def test_restart_after_sigkill(self):
        first = self.start()
        self.wait_ready(first)
        identity = self.identity()
        first[0].kill()
        self.assertEqual(first[0].wait(timeout=10), -signal.SIGKILL)
        self.assertEqual(self.identity(), identity)
        self.assertTrue(stat.S_ISSOCK(self.socket_path.lstat().st_mode))

        restarted = self.start()
        self.wait_ready(restarted)
        self.stop(restarted)
        # Positive control: normal shutdown removes its own socket.
        self.assertFalse(self.socket_path.exists())

    def test_second_instance_preserves_live_listener(self):
        owner = self.start()
        self.wait_ready(owner)
        identity = self.identity()
        output = self.assert_start_failed(self.start())
        self.assertEqual(self.identity(), identity)
        self.wait_ready(owner)
        self.assertIn(
            f"unix socket '{self.socket_path}' appears to be owned by "
            "another odyssey instance (lock held)", output)
        self.assertNotIn("failed to prepare unix socket", output)

    def test_regular_file_is_preserved(self):
        contents = b"This is a regular file, not an Odyssey socket.\n"
        self.socket_path.write_bytes(contents)
        self.socket_path.chmod(0o640)
        identity = self.identity()
        output = self.assert_start_failed(self.start())
        self.assertEqual(self.identity(), identity)
        self.assertEqual(self.socket_path.read_bytes(), contents)
        self.assertEqual(stat.S_IMODE(self.socket_path.lstat().st_mode), 0o640)
        self.assertIn(f"failed to prepare unix socket '{self.socket_path}': "
                      f"{os.strerror(errno.EEXIST)}", output)

    def test_shutdown_preserves_replacement_socket(self):
        owner = self.start()
        self.wait_ready(owner)
        original_identity = self.identity()
        with socket.socket(socket.AF_UNIX) as replacement:
            replacement.settimeout(5)
            replacement_path = self.root / "replacement.sock"
            replacement.bind(str(replacement_path))
            replacement.listen(1)
            # Rename a separately bound socket to avoid inode reuse.
            replacement_path.replace(self.socket_path)
            identity = self.identity()
            self.assertNotEqual(identity, original_identity)
            self.stop(owner)
            self.assertEqual(self.identity(), identity)
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(5)
                client.connect(str(self.socket_path))
                with replacement.accept()[0] as accepted:
                    accepted.sendall(b"replacement")
                    self.assertEqual(client.recv(11), b"replacement")


if __name__ == "__main__":
    unittest.main(verbosity=2)
