#!/usr/bin/env python3

"""Clients, GC and shutdown must remain usable during slow RELOAD file I/O."""

import argparse
import errno
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time


TIMEOUT = 5
HBA = b"host all all 127.0.0.1/32 trust\n"


def config_text(workdir, workers, port, log_debug):
    return f"""
daemonize no
enable_online_restart no
bindwith_reuseport no
log_format "%p %t %l (%c) %m\\n"
log_to_stdout yes
log_debug {log_debug}
log_session no
log_stats no
graceful_shutdown_timeout_ms 2000
locks_dir "{workdir}"
workers {workers}
resolvers 1
hba_file "{workdir}/hba.conf"

listen {{
    host "127.0.0.1"
    port {port}
}}

storage "local" {{
    type "local"
}}

database "console" {{
    user default {{
        authentication "none"
        role "admin"
        pool "session"
        storage "local"
    }}
}}
"""


def console_command(port, query):
    return [
        "psql", "-X", "-w", "-h", "127.0.0.1", "-p", str(port),
        "-U", "console", "-d", "console", "-v", "ON_ERROR_STOP=1",
        "--quiet", "--no-align", "--tuples-only", "-F", "|", "-c", query,
    ]


def console(port, query):
    result = subprocess.run(
        console_command(port, query), capture_output=True, text=True,
        timeout=TIMEOUT, check=True,
    )
    return result.stdout.strip()


def error_details(error):
    if isinstance(error, subprocess.CalledProcessError):
        return error.stderr.strip() if error.stderr else str(error)
    return str(error)


def expect_config(port, key, value):
    row = console(port, f"show config {key}")
    assert row.split("|")[0:2] == [key, value], f"unexpected config: {row!r}"


def expect_load_failed(port, value):
    rows = console(port, "show instance").splitlines()
    assert f"config_load_failed|{value}" in rows, f"unexpected instance state: {rows!r}"


def expect_log(path, message):
    assert message in path.read_text(), f"expected log message: {message}"


def wait_for_check(check):
    deadline = time.monotonic() + TIMEOUT
    while True:
        try:
            check()
            return
        except AssertionError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.05)


def wait_ready(process, port):
    deadline = time.monotonic() + TIMEOUT
    while time.monotonic() < deadline:
        assert process.poll() is None, "Odyssey exited during startup"
        try:
            console(port, "show config workers")
            return
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            time.sleep(0.05)
    raise AssertionError("Odyssey did not become ready")


def open_fifo_writer(path, process):
    deadline = time.monotonic() + TIMEOUT
    while time.monotonic() < deadline:
        assert process.poll() is None, "Odyssey exited during reload"
        try:
            # ENXIO until the reload reader opens this FIFO. After the open
            # succeeds, keep it empty and open so fgets() cannot reach EOF.
            return os.open(path, os.O_WRONLY | os.O_NONBLOCK)
        except OSError as error:
            if error.errno != errno.ENXIO:
                raise
            time.sleep(0.05)
    raise AssertionError(f"reload did not open the FIFO: {path}")


def wait_fifo_open(process):
    # Opening a writer would immediately release fopen(), and the config
    # reader's fseek() would then reject the FIFO. Observe the Linux FIFO-open
    # wait instead; search all threads so this also works after offloading I/O.
    tasks = Path(f"/proc/{process.pid}/task")
    deadline = time.monotonic() + TIMEOUT
    while time.monotonic() < deadline:
        assert process.poll() is None, "Odyssey exited during reload"
        for path in tasks.glob("*/wchan"):
            try:
                if path.read_text().strip() == "wait_for_partner":
                    return
            except FileNotFoundError:
                # A thread can exit between glob() and read_text().
                pass
        time.sleep(0.05)
    raise AssertionError("could not observe blocked FIFO open in /proc/<pid>/task/*/wchan")


def stop(process, expected_exit=0):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=TIMEOUT)
            raise AssertionError("Odyssey did not shut down after releasing the FIFO")
    assert process.returncode == expected_exit, f"Odyssey exited with {process.returncode}"

    # Match ody-stop's sanitizer checks even though we reap our own child.
    for options, fallback in (("ASAN_OPTIONS", "/asan-output.log"),
                              ("TSAN_OPTIONS", "/tsan-output.log")):
        prefix = fallback
        for option in os.environ.get(options, "").split():
            for setting in option.split(":"):
                if setting.startswith("log_path="):
                    prefix = setting.removeprefix("log_path=")
        report = Path(f"{prefix}.{process.pid}")
        if report.is_file():
            content = report.read_text()
            if options == "ASAN_OPTIONS":
                assert "ERROR" not in content and content.count("WARNING") <= 1, content
            else:
                assert not content, content


def run_case(odyssey, workers, trigger, port, source):
    label = f"source={source}, workers={workers}, trigger={trigger}"
    print(label, flush=True)
    with tempfile.TemporaryDirectory(prefix="odyssey-reload-slow-io-") as tmp:
        workdir = Path(tmp)
        config = workdir / "odyssey.conf"
        hba = workdir / "hba.conf"
        autoconf = workdir / "odyssey.conf.autoconf"
        logfile = workdir / "odyssey.log"
        config.write_text(config_text(workdir, workers, port, "no"))
        hba.write_bytes(HBA)

        with logfile.open("w") as log:
            process = subprocess.Popen([odyssey, str(config)], stdout=log, stderr=log)
            reload_process = None
            writer = None
            try:
                wait_ready(process, port)
                expect_config(port, "workers", str(workers))
                expect_config(port, "log_debug", "no")
                expect_load_failed(port, 0)
                config.write_text(config_text(workdir, workers, port, "yes"))
                if source == "hba":
                    hba.unlink()
                    os.mkfifo(hba)
                else:
                    os.mkfifo(autoconf)

                if trigger == "console":
                    reload_process = subprocess.Popen(
                        console_command(port, "reload"), stdout=log, stderr=log,
                    )
                else:
                    process.send_signal(signal.SIGHUP)

                if source == "hba":
                    writer = open_fifo_writer(hba, process)
                else:
                    wait_fifo_open(process)
                blocked_error = None
                try:
                    # Each command establishes a fresh PostgreSQL connection.
                    # A TCP connect alone could succeed against the backlog.
                    expect_config(port, "workers", str(workers))
                    expect_config(port, "log_debug", "no")
                    expect_load_failed(port, 0)
                    console(port, "gc")
                    if reload_process is not None:
                        assert reload_process.poll() is None, "RELOAD completed before FIFO release"
                except (AssertionError, subprocess.CalledProcessError,
                        subprocess.TimeoutExpired) as error:
                    blocked_error = error
                finally:
                    if source == "hba":
                        os.write(writer, HBA)
                    else:
                        writer = open_fifo_writer(autoconf, process)
                    os.close(writer)
                    writer = None

                if reload_process is not None:
                    reload_process.wait(timeout=TIMEOUT)
                    assert reload_process.returncode == 0, "RELOAD command failed"

                if source == "autoconf":
                    # The seekable config reader must reject a FIFO after
                    # fopen() returns, keeping the running config intact.
                    wait_for_check(lambda: expect_load_failed(port, 1))
                    expect_config(port, "log_debug", "no")
                    wait_for_check(lambda: expect_log(
                        logfile, f"failed to seek config file '{autoconf}'",
                    ))
                    print(f"{label}: failed reload kept the running config", flush=True)

                    # A subsequent reload of a regular autoconf must work
                    # and clear the failure state. Set the main config back
                    # to 'no' to verify that the autoconf override is applied.
                    config.write_text(config_text(workdir, workers, port, "no"))
                    autoconf.unlink()
                    autoconf.write_text("log_debug yes\n")
                    if trigger == "console":
                        console(port, "reload")
                    else:
                        process.send_signal(signal.SIGHUP)

                # SIGHUP has no reply; observe application via live state.
                wait_for_check(lambda: expect_config(port, "log_debug", "yes"))
                wait_for_check(lambda: expect_load_failed(port, 0))
                expect_config(port, "workers", str(workers))
                print(
                    f"{label}: reload applied and new clients work after FIFO release", flush=True,
                )
                if blocked_error is not None:
                    raise AssertionError(
                        f"new client could not query while reload was waiting on {source}: "
                        f"{error_details(blocked_error)}"
                    ) from blocked_error
            except Exception:
                print(logfile.read_text(), flush=True)
                raise
            finally:
                # Release a reader even if a probe or assertion failed, and
                # reap only the processes started by this test.
                try:
                    if writer is not None:
                        os.close(writer)
                    fifo = hba if source == "hba" else autoconf
                    if fifo.is_fifo():
                        # Also release fopen() if synchronization itself failed.
                        try:
                            writer = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
                        except OSError as error:
                            if error.errno != errno.ENXIO:
                                raise
                        else:
                            os.close(writer)
                finally:
                    try:
                        stop(process)
                    finally:
                        if reload_process is not None and reload_process.poll() is None:
                            reload_process.kill()
                            reload_process.wait(timeout=TIMEOUT)


class ReloadScenario:
    """Own only the processes and FIFOs created by one lifecycle test."""

    def __init__(self, odyssey, workers, port):
        self.odyssey = odyssey
        self.workers = workers
        self.port = port
        self.children = []
        self.writers = []

    def __enter__(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="odyssey-reload-lifecycle-")
        self.workdir = Path(self.tmp.name)
        self.config = self.workdir / "odyssey.conf"
        self.hba = self.workdir / "hba.conf"
        self.logfile = self.workdir / "odyssey.log"
        self.write_config("no")
        self.hba.write_bytes(HBA)
        self.log = self.logfile.open("w")
        self.process = subprocess.Popen(
            [self.odyssey, str(self.config)], stdout=self.log, stderr=self.log,
        )
        # Startup is checked in the body, so __exit__ also handles its failure.
        return self

    def __exit__(self, kind, error, traceback):
        if error is not None:
            print(self.logfile.read_text(), flush=True)
        try:
            for writer in self.writers:
                os.close(writer)
            if self.hba.is_fifo():
                try:
                    writer = os.open(self.hba, os.O_WRONLY | os.O_NONBLOCK)
                except OSError as failure:
                    if failure.errno != errno.ENXIO:
                        raise
                else:
                    os.close(writer)
            # Timeout cases assert their expected exit explicitly in the body.
            stop(self.process, expected_exit=124 if self.process.returncode == 124 else 0)
        finally:
            for child in self.children:
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=TIMEOUT)
            self.log.close()
            self.tmp.cleanup()

    def write_config(self, debug):
        self.config.write_text(config_text(self.workdir, self.workers, self.port, debug))

    def reload(self):
        child = subprocess.Popen(
            console_command(self.port, "reload"), stdout=self.log, stderr=self.log,
        )
        self.children.append(child)
        return child

    def hold_hba(self):
        writer = open_fifo_writer(self.hba, self.process)
        self.writers.append(writer)
        return writer

    def release_hba(self, writer, replace=False):
        if replace:
            self.hba.unlink()
            self.hba.write_bytes(HBA)
        os.write(writer, HBA)
        os.close(writer)
        self.writers.remove(writer)

    def expect_count(self, message, count):
        actual = self.logfile.read_text().count(message)
        assert actual == count, f"expected {count} occurrences of {message!r}, got {actual}"


def run_queue_case(odyssey, workers, port):
    label = f"queue and disconnected waiter, workers={workers}"
    print(label, flush=True)
    with ReloadScenario(odyssey, workers, port) as case:
        wait_ready(case.process, port)
        case.write_config("yes")
        case.hba.unlink()
        os.mkfifo(case.hba)
        first = case.reload()
        writer = case.hold_hba()

        # The first candidate has already read 'yes'. The queued one must
        # read the newer file only after the first candidate is applied.
        case.write_config("no")
        second = case.reload()
        wait_for_check(lambda: case.expect_count("RELOAD command received", 2))
        console(port, "gc")
        case.expect_count("importing changes from", 1)
        assert first.poll() is None and second.poll() is None
        expect_config(port, "log_debug", "no")

        case.release_hba(writer)
        first.wait(timeout=TIMEOUT)
        assert first.returncode == 0
        wait_fifo_open(case.process)
        expect_config(port, "log_debug", "yes")
        assert second.poll() is None, "queued RELOAD replied before its own preparation"
        writer = case.hold_hba()

        disconnected = case.reload()
        wait_for_check(lambda: case.expect_count("RELOAD command received", 3))
        disconnected.kill()
        disconnected.wait(timeout=TIMEOUT)
        console(port, "gc")
        case.expect_count("importing changes from", 2)

        case.release_hba(writer, replace=True)
        second.wait(timeout=TIMEOUT)
        assert second.returncode == 0
        # A fourth request serves as a barrier after the disconnected third.
        console(port, "reload")
        wait_for_check(lambda: case.expect_count("importing changes from", 4))
        expect_config(port, "log_debug", "no")
        expect_load_failed(port, 0)
        stop(case.process)
    print(f"{label}: passed", flush=True)


def run_shutdown_case(odyssey, workers, trigger, port, release):
    label = f"shutdown, workers={workers}, trigger={trigger}, release={release}"
    print(label, flush=True)
    with ReloadScenario(odyssey, workers, port) as case:
        wait_ready(case.process, port)
        case.write_config("yes")
        case.hba.unlink()
        os.mkfifo(case.hba)
        if trigger == "console":
            active = case.reload()
        else:
            active = None
            case.process.send_signal(signal.SIGHUP)
        writer = case.hold_hba()
        queued = case.reload()
        count = 2 if active is not None else 1
        wait_for_check(lambda: case.expect_count("RELOAD command received", count))
        console(port, "gc")
        case.process.terminate()
        wait_for_check(lambda: expect_log(case.logfile, "shutdown request"))
        queued.wait(timeout=TIMEOUT)
        assert queued.returncode != 0, "queued RELOAD reported success during shutdown"
        if active is not None:
            active.wait(timeout=TIMEOUT)
            assert active.returncode != 0, "active RELOAD reported success during shutdown"
        case.expect_count("importing changes from", 1)
        if release:
            # The successfully prepared late candidate must be discarded.
            case.release_hba(writer, replace=True)
            case.process.wait(timeout=TIMEOUT)
            stop(case.process)
            assert "reconfigure rules" not in case.logfile.read_text(), "late candidate applied"
        else:
            # Keep the FIFO blocked: SIGTERM must start the existing timer.
            case.process.wait(timeout=TIMEOUT)
            stop(case.process, expected_exit=124)
    print(f"{label}: passed", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--odyssey", default="odyssey")
    parser.add_argument("--port", type=int, default=6432)
    parser.add_argument("--source", choices=("hba", "autoconf"))
    args = parser.parse_args()
    os.environ["PGSSLMODE"] = "disable"
    os.environ["PGCONNECT_TIMEOUT"] = "2"

    failures = []
    sources = (args.source,) if args.source else ("hba", "autoconf")
    for source in sources:
        for workers in (1, 2):
            for trigger in ("console", "sighup"):
                try:
                    run_case(args.odyssey, workers, trigger, args.port, source)
                except Exception as error:
                    failures.append(
                        f"source={source}, workers={workers}, trigger={trigger}: "
                        f"{error_details(error)}"
                    )
    for workers in (1, 2):
        try:
            run_queue_case(args.odyssey, workers, args.port)
        except Exception as error:
            failures.append(f"queue, workers={workers}: {error_details(error)}")
        for trigger in ("console", "sighup"):
            for release in (True, False):
                try:
                    run_shutdown_case(args.odyssey, workers, trigger, args.port, release)
                except Exception as error:
                    failures.append(
                        f"shutdown, workers={workers}, trigger={trigger}, release={release}: "
                        f"{error_details(error)}"
                    )
    if failures:
        raise SystemExit("\n".join(failures))


if __name__ == "__main__":
    main()
