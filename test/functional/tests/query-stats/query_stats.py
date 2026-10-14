"""Query statistics over the PostgreSQL wire protocol, in both pstmt modes.

Run through runner.sh after the functional harness prepares PostgreSQL.
The filename deliberately avoids pytest's automatic test discovery.
"""

import argparse
import contextlib
import pathlib
import socket
import struct
import subprocess
import tempfile
import time
import unittest


def packet(kind, body=b""):
    return kind.encode() + struct.pack("!I", len(body) + 4) + body


def parse(sql, statement=""):
    return packet("P", statement.encode() + b"\0" + sql.encode() + b"\0\0\0")


def bind(portal="", statement=""):
    return packet("B", portal.encode() + b"\0" + statement.encode() + b"\0" * 7)


def execute(portal="", limit=0):
    return packet("E", portal.encode() + b"\0" + struct.pack("!I", limit))


class Client:
    def __init__(self, port, database, user):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=10)
        body = (struct.pack("!I", 196608) + b"user\0" + user.encode()
                + b"\0database\0" + database.encode() + b"\0\0")
        self.send(struct.pack("!I", len(body) + 4) + body)
        self.until("Z")

    def close(self):
        self.sock.close()

    def send(self, data):
        self.sock.sendall(data)

    def read_exact(self, size):
        data = b""
        while len(data) < size:
            chunk = self.sock.recv(size - len(data))
            if not chunk:
                raise RuntimeError("Unexpected PostgreSQL EOF")
            data += chunk
        return data

    def until(self, stop):
        messages = []
        while True:
            kind = self.read_exact(1).decode()
            size = struct.unpack("!I", self.read_exact(4))[0] - 4
            body = self.read_exact(size)
            messages.append((kind, body))
            if kind == stop:
                return messages

    def query(self, sql):
        self.send(packet("Q", sql.encode() + b"\0"))
        columns, rows = [], []
        for kind, body in self.until("Z"):
            if kind == "E":
                raise RuntimeError(body)
            if kind == "T":
                pos = 2
                for _ in range(struct.unpack("!H", body[:2])[0]):
                    end = body.index(0, pos)
                    columns.append(body[pos:end].decode())
                    pos = end + 19
            elif kind == "D":
                pos, values = 2, []
                for _ in range(struct.unpack("!H", body[:2])[0]):
                    size = struct.unpack("!i", body[pos:pos + 4])[0]
                    pos += 4
                    values.append(body[pos:pos + size].decode() if size >= 0 else None)
                    if size >= 0:
                        pos += size
                rows.append(dict(zip(columns, values)))
        return rows


@contextlib.contextmanager
def odyssey(reserve):
    with tempfile.TemporaryDirectory(prefix="odyssey-query-stats-") as directory:
        root = pathlib.Path(directory)
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        (root / "locks").mkdir()
        config = root / "odyssey.conf"
        config.write_text(f'''
daemonize no
log_format "%p %t %l (%c) %m\\n"
log_to_stdout yes
stats_interval 3600
workers 1
resolvers 1
locks_dir "{root}/locks"
unix_socket_dir "{root}"
unix_socket_mode "0700"
listen {{ host "127.0.0.1" port {port} }}
storage "pg" {{ type "remote" host "{OPTIONS.backend_host}" port {OPTIONS.backend_port} }}
storage "local" {{ type "local" }}
database default {{ user default {{
    authentication "none"
    storage "pg"
    storage_user "{OPTIONS.backend_user}"
    pool "transaction"
    pool_size 1
    pool_discard no
    pool_smart_discard no
    pool_reserve_prepared_statement {'yes' if reserve else 'no'}
    quantiles "0,0.5,1"
}} }}
database "console" {{ user "console" {{
    authentication "none"
    role "admin"
    storage "local"
    pool "session"
    quantiles "0,0.5,1"
}} }}
''')
        clients = []
        with (root / "odyssey.log").open("w") as log:
            process = subprocess.Popen([OPTIONS.odyssey_bin, str(config)], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            while True:
                try:
                    console = Client(port, "console", "console")
                    clients.append(console)
                    break
                except ConnectionRefusedError:
                    if process.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError((root / "odyssey.log").read_text())
                    time.sleep(0.02)
            client = Client(port, "postgres", OPTIONS.backend_user)
            clients.append(client)
            yield client, console
        except BaseException:
            print((root / "odyssey.log").read_text())
            raise
        finally:
            for client in clients:
                client.close()
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


def stats(console):
    for row in console.query("show stats"):
        if row["database"] == "postgres":
            return {key: int(value) for key, value in row.items() if key.startswith("total_")}
    return dict(total_query_count=0, total_query_time=0, total_xact_count=0, total_xact_time=0)


def both_modes(method):
    def run(self):
        for reserve in (False, True):
            with self.subTest(reserve_prepared_statement=reserve):
                with odyssey(reserve) as (client, console):
                    method(self, client, console, reserve)
    return run


class QueryStats(unittest.TestCase):
    @both_modes
    def test_each_execute_in_batch_has_a_sample(self, client, console, reserve):
        batch = b""
        for i in range(3):
            name = str(i)
            batch += parse("select pg_sleep(0.02)", name) + bind(name, name) + execute(name)
        client.send(batch + packet("S"))
        messages = client.until("Z")
        self.assertEqual(sum(kind == "C" for kind, _ in messages), 3)
        result = stats(console)
        self.assertEqual(result["total_query_count"], 3)
        self.assertEqual(result["total_xact_count"], 1)
        # A common send timestamp includes the queue: >=20+40+60 milliseconds.
        self.assertGreaterEqual(result["total_query_time"], 110000)
        pool = next(row for row in console.query("show pools_extended") if row["database"] == "postgres")
        self.assertGreaterEqual(int(pool["query_0"]), 19000)
        self.assertGreaterEqual(int(pool["query_0.5"]), 39000)
        self.assertGreaterEqual(int(pool["query_1"]), 59000)

    @both_modes
    def test_sync_and_parse_do_not_count_as_queries(self, client, console, reserve):
        client.send(packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_count"], 0)
        client.send(parse("select 1") + packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_count"], 0)

    @both_modes
    def test_flush_records_query_before_sync(self, client, console, reserve):
        client.send(parse("select 1") + bind() + execute() + packet("H"))
        client.until("C")
        before = stats(console)
        self.assertEqual(before["total_query_count"], 1)
        self.assertEqual(before["total_xact_count"], 0)
        time.sleep(0.25)
        client.send(packet("S"))
        client.until("Z")
        after = stats(console)
        self.assertEqual(after["total_query_count"], 1)
        self.assertEqual(after["total_query_time"], before["total_query_time"])
        self.assertEqual(after["total_xact_count"], 1)
        self.assertGreater(after["total_xact_time"], after["total_query_time"] + 200000)

    @both_modes
    def test_new_flush_batch_gets_a_new_timestamp(self, client, console, reserve):
        client.send(parse("select 1") + packet("H"))
        client.until("1")
        self.assertEqual(stats(console)["total_query_count"], 0)
        time.sleep(0.25)
        client.send(bind() + execute() + packet("S"))
        client.until("Z")
        result = stats(console)
        self.assertEqual(result["total_query_count"], 1)
        self.assertGreater(result["total_xact_time"], result["total_query_time"] + 200000)

    @both_modes
    def test_suspended_portal_counts_each_execute(self, client, console, reserve):
        client.send(parse("select generate_series(1,2)") + bind() + execute(limit=1) + packet("H"))
        client.until("s")
        self.assertEqual(stats(console)["total_query_count"], 1)
        client.send(execute() + packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_count"], 2)

    @both_modes
    def test_execute_error_counts_but_skipped_execute_does_not(self, client, console, reserve):
        client.send(parse("select 1/(i-i) from generate_series(1,1) i", "bad")
                    + bind("bad", "bad") + execute("bad")
                    + parse("select 1", "good") + bind("good", "good") + execute("good")
                    + packet("H"))
        client.until("E")
        before = stats(console)
        self.assertEqual(before["total_query_count"], 1)
        client.send(packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_time"], before["total_query_time"])

    @both_modes
    def test_parse_and_bind_errors_do_not_count_execute(self, client, console, reserve):
        client.send(parse("select from") + bind() + execute() + packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_count"], 0)
        client.send(parse("select $1::int") + bind() + execute() + packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_count"], 0)

    @both_modes
    def test_empty_query_response_counts_execute(self, client, console, reserve):
        client.send(parse("") + bind() + execute() + packet("S"))
        self.assertIn("I", [kind for kind, _ in client.until("Z")])
        self.assertEqual(stats(console)["total_query_count"], 1)

    @both_modes
    def test_virtual_execute_is_counted(self, client, console, reserve):
        client.send(parse("DEALLOCATE ALL") + bind() + execute() + packet("H"))
        client.until("C")
        self.assertEqual(stats(console)["total_query_count"], 1)
        client.send(packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_count"], 1)

    @both_modes
    def test_error_skips_virtual_execute(self, client, console, reserve):
        client.send(parse("select 1", "a") + parse("DEALLOCATE ALL", "d") + packet("S"))
        client.until("Z")
        client.send(bind("p", "a") + execute("p") + bind("p", "a")
                    + bind("q", "d") + execute("q") + packet("S"))
        messages = client.until("Z")
        self.assertEqual([kind for kind, _ in messages], ["2", "D", "C", "E", "Z"])
        self.assertEqual(stats(console)["total_query_count"], 1)

    @both_modes
    def test_simple_query_and_explicit_transaction_keep_their_counts(self, client, console, reserve):
        # Odyssey defers BEGIN locally; it has never counted as a query.
        client.query("BEGIN")
        client.send(parse("select 1") + bind() + execute() + execute() + packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_count"], 2)
        self.assertEqual(stats(console)["total_xact_count"], 0)
        client.query("COMMIT")
        self.assertEqual(stats(console)["total_query_count"], 3)
        self.assertEqual(stats(console)["total_xact_count"], 1)
        client.query("SELECT 1")
        self.assertEqual(stats(console)["total_query_count"], 4)
        self.assertEqual(stats(console)["total_xact_count"], 2)

    @both_modes
    def test_copy_out_counts_execute(self, client, console, reserve):
        client.send(parse("COPY (SELECT 1) TO STDOUT") + bind() + execute() + packet("S"))
        self.assertIn("H", [kind for kind, _ in client.until("Z")])
        self.assertEqual(stats(console)["total_query_count"], 1)

    @both_modes
    def test_copy_in_counts_execute(self, client, console, reserve):
        client.query("BEGIN; CREATE TEMP TABLE query_stats_copy (i int)")
        client.send(parse("COPY query_stats_copy FROM STDIN") + bind() + execute() + packet("H"))
        client.until("G")
        self.assertEqual(stats(console)["total_query_count"], 1)
        client.send(packet("d", b"1\n") + packet("c"))
        client.until("C")
        self.assertEqual(stats(console)["total_query_count"], 2)
        client.send(packet("S"))
        client.until("Z")
        self.assertEqual(stats(console)["total_query_count"], 2)
        client.query("ROLLBACK")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--odyssey-bin", default="/usr/bin/odyssey")
    parser.add_argument("--backend-host", default="127.0.0.1")
    parser.add_argument("--backend-port", type=int, default=5432)
    parser.add_argument("--backend-user", default="postgres")
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__] + remaining)
