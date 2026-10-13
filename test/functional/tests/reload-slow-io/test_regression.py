#!/usr/bin/env python3

"""Exercise reload application, TLS session lifetime and concurrent requests."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import os
import signal
import socket
import ssl
import struct
import subprocess
import threading

from test_reload import (
    HBA, TIMEOUT, ReloadScenario, console, expect_config, expect_load_failed,
    open_fifo_writer, stop, wait_fifo_open, wait_for_check, wait_ready,
)


class PgError(Exception):
    pass


class PgClient:
    """Minimal PostgreSQL client for authentication-none and persistent TLS.

    psql covers normal console commands; this helper keeps a connection open
    across reloads and exposes the certificate actually received by a client.
    """

    def __init__(self, port, user="console", database="console", tls=False):
        self.sock = socket.create_connection(("127.0.0.1", port), TIMEOUT)
        self.certificate = None
        try:
            if tls:
                self.sock.sendall(struct.pack("!II", 8, 80877103))
                assert self.read(1) == b"S", "listener refused TLS"
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
                context.check_hostname = False
                context.verify_mode = ssl.CERT_NONE
                self.sock = context.wrap_socket(self.sock, server_hostname="localhost")
                self.certificate = self.sock.getpeercert(binary_form=True)
            params = f"user\0{user}\0database\0{database}\0client_encoding\0UTF8\0\0".encode()
            self.sock.sendall(struct.pack("!II", len(params) + 8, 196608) + params)
            while True:
                kind, body = self.message()
                if kind == b"E":
                    raise PgError(self.error(body))
                if kind == b"R":
                    assert body == struct.pack("!I", 0), "unexpected authentication method"
                if kind == b"Z":
                    break
        except BaseException:
            self.sock.close()
            raise

    def read(self, size):
        data = bytearray()
        while len(data) < size:
            chunk = self.sock.recv(size - len(data))
            if not chunk:
                raise EOFError("Odyssey closed the PostgreSQL connection")
            data.extend(chunk)
        return bytes(data)

    def message(self):
        kind = self.read(1)
        size = struct.unpack("!I", self.read(4))[0]
        assert 4 <= size <= 1024 * 1024, f"invalid message size: {size}"
        return kind, self.read(size - 4)

    @staticmethod
    def error(body):
        return next((field[1:].decode() for field in body.split(b"\0")
                     if field.startswith(b"M")), repr(body))

    def query(self, query):
        body = query.encode() + b"\0"
        self.sock.sendall(b"Q" + struct.pack("!I", len(body) + 4) + body)
        rows = []
        error = None
        while True:
            kind, body = self.message()
            if kind == b"E":
                error = self.error(body)
            elif kind == b"D":
                count = struct.unpack_from("!H", body)[0]
                offset = 2
                row = []
                for _ in range(count):
                    size = struct.unpack_from("!i", body, offset)[0]
                    offset += 4
                    row.append(None if size == -1 else body[offset:offset + size].decode())
                    if size != -1:
                        offset += size
                rows.append(row)
            elif kind == b"Z":
                if error is not None:
                    raise PgError(error)
                return rows

    def __enter__(self):
        return self

    def __exit__(self, *_):
        try:
            self.sock.sendall(b"X" + struct.pack("!I", 4))
        except OSError:
            pass
        finally:
            self.sock.close()


def atomic_write(path, text):
    temporary = path.with_suffix(path.suffix + ".next")
    temporary.write_text(text)
    temporary.replace(path)


def probe_rule(user):
    return f'''
database "probe" {{
    user "{user}" {{
        authentication "none"
        role "admin"
        pool "session"
        storage "local"
    }}
}}
'''


def expect_rejected(port, user, message):
    try:
        with PgClient(port, user, "probe"):
            pass
    except PgError as error:
        assert message in str(error), f"unexpected rejection: {error}"
    else:
        raise AssertionError(f"connection for {user} unexpectedly succeeded")


def trigger_reload(case, trigger, failed, debug):
    message = (f"failed to load '{case.config}', keeping the running configuration"
               if failed else "reconfigure rules")
    before_completion = case.logfile.read_text().count(message)
    if trigger == "console":
        console(case.port, "reload")
    else:
        before = case.logfile.read_text().count("SIGHUP received")
        case.process.send_signal(signal.SIGHUP)
        wait_for_check(lambda: case.expect_count("SIGHUP received", before + 1))
    wait_for_check(lambda: case.expect_count(message, before_completion + 1))
    wait_for_check(lambda: expect_load_failed(case.port, failed))
    wait_for_check(lambda: expect_config(case.port, "log_debug", debug))


def run_application_case(binary, workers, trigger, port):
    label = f"config/rules/HBA, workers={workers}, trigger={trigger}"
    print(label, flush=True)
    with ReloadScenario(binary, workers, port) as case:
        wait_ready(case.process, port)
        base = case.config.read_text()
        atomic_write(case.config, base + probe_rule("alice"))
        trigger_reload(case, trigger, 0, "no")
        with PgClient(port, "alice", "probe") as client:
            assert client.query("show config workers")[0][:2] == ["workers", str(workers)]

        changed = base.replace("log_debug no", "log_debug yes") + probe_rule("bob")
        atomic_write(case.config, changed)
        trigger_reload(case, trigger, 0, "yes")
        expect_rejected(port, "alice", "is not found")
        with PgClient(port, "bob", "probe") as client:
            assert client.query("show config log_debug")[0][:2] == ["log_debug", "yes"]

        atomic_write(case.hba, "host probe bob 127.0.0.1/32 deny\n" + HBA.decode())
        console(port, "reload")
        expect_rejected(port, "bob", "host based authentication rejected")
        console(port, "gc")
        atomic_write(case.hba, HBA.decode())
        console(port, "reload")
        with PgClient(port, "bob", "probe") as client:
            assert client.query("show config log_debug")[0][1] == "yes"

        # Parser errors and HBA errors must leave the live rules/config intact.
        atomic_write(case.config, "this is not a valid Odyssey config\n")
        trigger_reload(case, trigger, 1, "yes")
        with PgClient(port, "bob", "probe") as client:
            assert client.query("show config log_debug")[0][1] == "yes"
        atomic_write(case.config, base + probe_rule("bob"))
        trigger_reload(case, trigger, 0, "no")
        atomic_write(case.hba, "this is not a valid HBA rule\n")
        trigger_reload(case, trigger, 1, "no")
        with PgClient(port, "bob", "probe") as client:
            assert client.query("show config log_debug")[0][1] == "no"
        atomic_write(case.hba, HBA.decode())
        atomic_write(case.config, changed)
        trigger_reload(case, trigger, 0, "yes")
    print(f"{label}: passed", flush=True)


def make_certificate(case, name):
    cert, key = case.workdir / f"{name}.pem", case.workdir / f"{name}.key"
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
        "-keyout", str(key), "-out", str(cert), "-days", "2",
        "-subj", f"/CN=odyssey-{name}",
    ], check=True, capture_output=True, timeout=30)
    return cert, key, ssl.PEM_cert_to_DER_cert(cert.read_text())


def tls_config(base, cert, key, debug="no"):
    return base.replace("log_debug no", f"log_debug {debug}").replace(
        'listen {\n', f'listen {{\n    tls "allow"\n'
        f'    tls_cert_file "{cert}"\n    tls_key_file "{key}"\n',
    )


def expect_certificate(port, certificate):
    with PgClient(port, tls=True) as client:
        assert client.certificate == certificate, "listener served the wrong certificate"
        assert client.query("show config workers"), "TLS client could not query"


def run_tls_case(binary, workers, port):
    label = f"TLS rotation and old session, workers={workers}"
    print(label, flush=True)
    with ReloadScenario(binary, workers, port) as case:
        wait_ready(case.process, port)
        base = case.config.read_text()
        cert_a, key_a, der_a = make_certificate(case, "a")
        cert_b, key_b, der_b = make_certificate(case, "b")
        atomic_write(case.config, tls_config(base, cert_a, key_a))
        trigger_reload(case, "console", 0, "no")
        with PgClient(port, tls=True) as old:
            assert old.certificate == der_a
            for cert, key in ((case.workdir / "missing.pem", key_a), (cert_a, key_b)):
                atomic_write(case.config, tls_config(base, cert, key, "yes"))
                # A bad TLS replacement preserves the old TLS context while
                # the remaining valid configuration changes are still applied.
                trigger_reload(case, "console", 0, "yes")
                expect_certificate(port, der_a)
                assert old.query("show config workers")

            # Mandatory TLS treats a bad certificate as a candidate error.
            required = tls_config(base, case.workdir / "missing.pem", key_a)
            atomic_write(case.config, required.replace('tls "allow"', 'tls "require"'))
            trigger_reload(case, "console", 1, "yes")
            expect_certificate(port, der_a)
            assert old.query("show config log_debug")[0][1] == "yes"

            atomic_write(case.config, tls_config(base, cert_b, key_b, "yes"))
            trigger_reload(case, "sighup", 0, "yes")
            expect_certificate(port, der_b)
            assert old.query("show config log_debug")[0][1] == "yes"

            # Rotate contents at unchanged paths; cache invalidation must make
            # new connections see A again, while an existing B session survives.
            with PgClient(port, tls=True) as second:
                cert_b.write_bytes(cert_a.read_bytes())
                key_b.write_bytes(key_a.read_bytes())
                console(port, "reload")
                expect_certificate(port, der_a)
                assert second.query("show config workers")
                assert old.query("show config workers")

                fifo = case.workdir / "slow.pem"
                os.mkfifo(fifo)
                atomic_write(case.config, tls_config(base, fifo, key_a, "yes"))
                child = case.reload()
                try:
                    wait_fifo_open(case.process)
                    assert child.poll() is None
                    expect_certificate(port, der_a)
                    console(port, "gc")
                    assert old.query("show config workers")
                    assert second.query("show config workers")
                finally:
                    writer = open_fifo_writer(fifo, case.process)
                    try:
                        # Validation reads the certificate more than once.
                        # Keep the open FIFO reader, but provide a regular file
                        # for later opens before releasing the first reader.
                        fifo.unlink()
                        fifo.write_bytes(cert_a.read_bytes())
                        os.write(writer, cert_a.read_bytes())
                    finally:
                        os.close(writer)
                child.wait(timeout=TIMEOUT)
                assert child.returncode == 0
                expect_certificate(port, der_a)
                assert old.query("show config workers")
                assert second.query("show config workers")
    print(f"{label}: passed", flush=True)


def run_stress_case(binary, workers, port):
    label = f"concurrent clients/RELOAD/SIGHUP/GC, workers={workers}"
    print(label, flush=True)
    with ReloadScenario(binary, workers, port) as case:
        wait_ready(case.process, port)
        barrier = threading.Barrier(8, timeout=TIMEOUT)

        def clients():
            barrier.wait()
            for _ in range(60):
                with PgClient(port) as client:
                    assert client.query("show config workers")[0][1] == str(workers)

        def reloads():
            barrier.wait()
            for _ in range(20):
                with PgClient(port) as client:
                    client.query("reload")

        def gc():
            barrier.wait()
            for _ in range(40):
                with PgClient(port) as client:
                    client.query("gc")

        def signals():
            barrier.wait()
            for count in range(1, 13):
                case.process.send_signal(signal.SIGHUP)
                wait_for_check(lambda: case.expect_count("SIGHUP received", count))

        with ThreadPoolExecutor(max_workers=8) as executor:
            tasks = [executor.submit(clients) for _ in range(4)]
            tasks += [executor.submit(reloads) for _ in range(2)]
            tasks += [executor.submit(gc), executor.submit(signals)]
            for task in tasks:
                task.result(timeout=60)
        console(port, "reload")  # Barrier after all accepted requests.
        wait_for_check(lambda: case.expect_count("reconfigure rules", 53))
        expect_load_failed(port, 0)
        expect_config(port, "workers", str(workers))
    print(f"{label}: 240 query connections, 53 reloads, 40 GC passed", flush=True)


def run_shutdown_stress_case(binary, workers, port):
    label = f"concurrent shutdown with blocked loader, workers={workers}"
    print(label, flush=True)
    with ReloadScenario(binary, workers, port) as case:
        wait_ready(case.process, port)
        case.hba.unlink()
        os.mkfifo(case.hba)
        case.process.send_signal(signal.SIGHUP)
        writer = case.hold_hba()
        stopping = threading.Event()
        progress = [threading.Event(), threading.Event()]

        def reload():
            with PgClient(port) as client:
                try:
                    client.query("reload")
                except PgError:
                    assert stopping.is_set(), "RELOAD failed before shutdown"
                else:
                    raise AssertionError("blocked RELOAD reported success")

        def probe(index, query):
            count = 0
            while not stopping.is_set():
                try:
                    with PgClient(port) as client:
                        rows = client.query(query)
                        if index == 0:
                            assert rows[0][:2] == ["workers", str(workers)]
                        else:
                            assert rows == []
                except (PgError, OSError, EOFError):
                    if not stopping.is_set():
                        raise
                    break
                count += 1
                if count == 20:
                    progress[index].set()
            return count

        with ThreadPoolExecutor(max_workers=6) as executor:
            tasks = [executor.submit(reload) for _ in range(4)]
            probes = [executor.submit(probe, 0, "show config workers"),
                      executor.submit(probe, 1, "gc")]
            try:
                wait_for_check(lambda: case.expect_count("RELOAD command received", 4))
                for event in progress:
                    assert event.wait(TIMEOUT), "probes stalled behind reload"
                for count in range(2, 10):
                    case.process.send_signal(signal.SIGHUP)
                    wait_for_check(lambda: case.expect_count("SIGHUP received", count))
                stopping.set()
                case.process.terminate()
                for task in tasks:
                    task.result(timeout=TIMEOUT)
                for task in probes:
                    assert task.result(timeout=TIMEOUT) >= 20
                case.process.wait(timeout=TIMEOUT)
                stop(case.process, expected_exit=124)
                case.expect_count("importing changes from", 1)
            finally:
                stopping.set()
                # Cleanup can terminate normally even if an assertion failed.
                if case.process.poll() is None:
                    case.process.terminate()
                    case.release_hba(writer, replace=True)
    print(f"{label}: canceled 4 console and 9 signal requests, exited 124", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--odyssey", default="odyssey")
    parser.add_argument("--port", type=int, default=6432)
    parser.add_argument("--only", choices=("application", "tls", "stress", "shutdown"))
    args = parser.parse_args()
    os.environ["PGSSLMODE"] = "disable"
    os.environ["PGCONNECT_TIMEOUT"] = "2"
    for workers in (1, 2):
        if args.only in (None, "application"):
            for trigger in ("console", "sighup"):
                run_application_case(args.odyssey, workers, trigger, args.port)
        if args.only in (None, "tls"):
            run_tls_case(args.odyssey, workers, args.port)
        if args.only in (None, "stress"):
            run_stress_case(args.odyssey, workers, args.port)
        if args.only in (None, "shutdown"):
            run_shutdown_stress_case(args.odyssey, workers, args.port)


if __name__ == "__main__":
    main()
