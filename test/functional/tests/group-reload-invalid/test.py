#!/usr/bin/env python3

import argparse
import csv
import io
import os
from pathlib import Path
import pwd
import re
import shutil
import signal
import socket
import subprocess
import tempfile
import time


TIMEOUT = 15
ALLOW = "host all all 127.0.0.1/32 allow\n"
CASES = (
    "unknown", "syntax", "missing-main", "missing-include",
    "global-validation", "rule-validation", "partial-group", "partial-hba",
)


def unused_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class Regression:
    def __init__(self, args, work):
        self.args = args
        self.work = work
        self.config = work / "odyssey.conf"
        self.hba = work / "hba.conf"
        self.log = work / "odyssey.log"
        self.pg_port = unused_port()
        self.od_port = unused_port()
        while self.od_port == self.pg_port:
            self.od_port = unused_port()
        self.pg_started = False
        self.ody = None
        self.pg_prefix = []
        if os.geteuid() == 0:
            postgres = pwd.getpwnam("postgres")
            os.chown(work, postgres.pw_uid, postgres.pw_gid)
            self.pg_prefix = ["sudo", "-u", "postgres"]
        self.base = Path(__file__).with_name("config.conf").read_text()
        for key, value in (("WORK", work), ("PG_PORT", self.pg_port),
                           ("OD_PORT", self.od_port)):
            self.base = self.base.replace(f"@{key}@", str(value))

    def pg_tool(self, name, *args):
        return subprocess.run(
            self.pg_prefix + [str(self.args.pg_bin / name), *args],
            check=True, capture_output=True, text=True, timeout=30,
        )

    def sql(self, port, user, db, query, check=True, csv_output=False, timeout=5):
        result = subprocess.run(
            [str(self.args.pg_bin / "psql"), "-X", "-w", "-v",
             "ON_ERROR_STOP=1", "--csv" if csv_output else "-At",
             f"host=127.0.0.1 port={port} user={user} dbname={db} "
             "sslmode=disable connect_timeout=2", "-c", query],
            capture_output=True, text=True, timeout=timeout,
        )
        if check and result.returncode:
            raise AssertionError(f"{db}/{user}: {result.stderr}")
        return result

    def backend(self, query):
        return self.sql(self.pg_port, "postgres", "postgres", query, timeout=30)

    def admin(self, query, csv_output=False):
        return self.sql(self.od_port, "console", "console", query,
                        csv_output=csv_output, timeout=TIMEOUT)

    def alive(self):
        assert self.ody.poll() is None, f"Odyssey exited: {self.ody.returncode}"

    def wait(self, description, condition):
        deadline = time.monotonic() + TIMEOUT
        while time.monotonic() < deadline:
            self.alive()
            if condition():
                return
            time.sleep(0.05)
        raise AssertionError(f"Timeout waiting for {description}")

    def member(self, db, user, allowed):
        def matches():
            result = self.sql(self.od_port, user, db, "SELECT 1", check=False)
            if allowed:
                return result.returncode == 0 and result.stdout.strip() == "1"
            return result.returncode != 0 and "user blocked" in result.stderr
        self.wait(f"{db}/{user}: allowed={allowed}", matches)

    def members(self, alice_allowed, bob_allowed):
        for db in ("grouped", "grouped2"):
            self.member(db, "alice", alice_allowed)
            self.member(db, "bob", bob_allowed)

    def set_members(self, user):
        self.backend(
            "TRUNCATE members, members2; "
            f"INSERT INTO members VALUES ('{user}'); "
            f"INSERT INTO members2 VALUES ('{user}');"
        )

    def log_since(self, offset):
        with self.log.open("rb") as log:
            log.seek(offset)
            return log.read().decode(errors="replace")

    def reload(self, entry, failed, diagnostic=""):
        offset = self.log.stat().st_size
        if entry == "sql":
            # RELOAD's command tag does not report configuration failure.
            self.admin("RELOAD")
        else:
            self.ody.send_signal(signal.SIGHUP)
        marker = ("keeping the running configuration" if failed else
                  "routes created/deleted and scheduled for removal")
        # A previous config_load_failed=1 is not a barrier for another attempt.
        self.wait(f"{entry} reload completion", lambda:
                  marker in self.log_since(offset))
        rows = list(csv.DictReader(io.StringIO(
            self.admin("SHOW INSTANCE", csv_output=True).stdout)))
        values = [row["items"] for row in rows
                  if row["list"] == "config_load_failed"]
        assert values == [str(int(failed))], rows
        delta = self.log_since(offset)
        assert diagnostic in delta, (diagnostic, delta)
        return offset

    def start(self):
        self.pg_tool("initdb", "-D", str(self.work / "pg"), "-U", "postgres",
                     "-A", "trust", "--no-locale", "--encoding=UTF8")
        self.pg_tool("pg_ctl", "-D", str(self.work / "pg"), "-l",
                     str(self.work / "postgres.log"), "-o",
                     f"-h 127.0.0.1 -p {self.pg_port} -k {self.work}",
                     "-w", "start")
        self.pg_started = True
        self.backend("CREATE TABLE members (name text); "
                     "CREATE TABLE members2 (name text); "
                     "CREATE TABLE replacement (name text); "
                     "INSERT INTO replacement VALUES ('carol');")
        self.set_members("alice")
        self.hba.write_text(ALLOW)
        self.config.write_text(self.base)
        env = os.environ.copy()
        # Keep sanitizer settings, but collect diagnostics for this PID locally.
        for key in ("ASAN_OPTIONS", "TSAN_OPTIONS"):
            env[key] = env.get(key, "") + f":log_path={self.work}/{key}"
        with self.log.open("w") as log:
            self.ody = subprocess.Popen([str(self.args.odyssey), str(self.config)],
                                        stdout=log, stderr=subprocess.STDOUT,
                                        env=env)
        self.members(True, False)

    def reject(self, entry, case):
        self.set_members("alice")
        self.members(True, False)
        self.hba.write_text(ALLOW)
        config = self.base
        if case == "unknown":
            config += "invalid_reload_option yes\n"
            diagnostic = "unknown directive"
        elif case == "syntax":
            config += 'database "unfinished" {\n'
            diagnostic = "syntax error"
        elif case == "missing-main":
            diagnostic = "No such file"
        elif case == "missing-include":
            config += f'include "{self.work}/missing.conf"\n'
            diagnostic = "No such file"
        elif case == "global-validation":
            config = config.replace("enable_online_restart no",
                                    "enable_online_restart yes")
            diagnostic = "online restart feature works only with SO_REUSEPORT"
        elif case == "rule-validation":
            config = config.replace('        group_query_db "postgres"\n', "", 1)
            diagnostic = "group_query_db is not set"
        elif case == "partial-group":
            config = config.replace('    group "members" {',
                                    '    group "members" {\n        role "invalid"')
            diagnostic = "role type 'invalid' is unknown"
        elif case == "partial-hba":
            self.hba.write_text(ALLOW + "invalid all all 127.0.0.1/32 allow\n")
            diagnostic = "unknown connection type"
        if case in ("global-validation", "rule-validation"):
            # The rejected HBA must never deny the old member.
            self.hba.write_text("host grouped alice 127.0.0.1/32 deny\n" + ALLOW)
            # Exercise cleanup of an allocated but never started watchdog.
            config += f'''
storage "temporary" {{
    type "remote"
    host "127.0.0.1"
    port {self.pg_port}
    watchdog {{
        authentication "none"
        storage "temporary"
        storage_db "postgres"
        storage_user "postgres"
        pool "transaction"
        pool_routing "internal"
        watchdog_lag_query "SELECT 0"
        watchdog_lag_interval 1
    }}
}}
'''
        self.config.write_text(config)
        if case == "missing-main":
            self.config.unlink()
        offset = self.reload(entry, failed=True, diagnostic=diagnostic)
        self.members(True, False)
        self.set_members("bob")
        self.members(False, True)
        delta = self.log_since(offset)
        assert "start group checking" not in delta, delta
        assert not re.search(r"group checking .* finished", delta), delta
        print(f"PASS {entry}: {case}, preserved and updated both groups", flush=True)

    def recover(self, entry):
        self.hba.write_text(ALLOW)
        changed = re.sub(r"SELECT name FROM members2?", "SELECT name FROM replacement",
                         self.base)
        self.config.write_text(changed)
        offset = self.reload(entry, failed=False)
        for db in ("grouped", "grouped2"):
            self.member(db, "carol", True)
            self.member(db, "bob", False)
        self.wait("two replacement checkers", lambda:
                  self.log_since(offset).count("start group checking") == 2)
        removed = re.sub(r'    group "members2?" \{.*?\n    \}\n', "", changed,
                         flags=re.DOTALL)
        self.config.write_text(removed)
        offset = self.reload(entry, failed=False)
        for db in ("grouped", "grouped2"):
            self.member(db, "carol", False)
        assert len(re.findall(r"group checking .* finished",
                              self.log_since(offset))) == 2
        assert "start group checking" not in self.log_since(offset)
        self.config.write_text(self.base)
        self.set_members("alice")
        offset = self.reload(entry, failed=False)
        self.members(True, False)
        self.wait("two restored checkers", lambda:
                  self.log_since(offset).count("start group checking") == 2)
        self.set_members("bob")
        self.members(False, True)
        print(f"PASS {entry}: recovery, query change, removal and re-addition", flush=True)

    def shutdown(self):
        offset = self.log.stat().st_size
        self.ody.send_signal(signal.SIGTERM)
        assert self.ody.wait(timeout=TIMEOUT) == 0, self.ody.returncode
        assert len(re.findall(r"group checking .* finished",
                              self.log_since(offset))) == 2
        self.check_sanitizers()
        print("PASS graceful shutdown", flush=True)

    def check_sanitizers(self):
        for log in [self.log, *self.work.glob("*SAN_OPTIONS.*")]:
            data = log.read_text(errors="replace")
            assert not re.search(r"ERROR: (AddressSanitizer|LeakSanitizer)|"
                                 r"WARNING: ThreadSanitizer|runtime error:", data), data

    def cleanup(self):
        if self.ody is not None and self.ody.poll() is None:
            self.ody.terminate()
            try:
                self.ody.wait(timeout=TIMEOUT)
            except subprocess.TimeoutExpired:
                self.ody.kill()
                self.ody.wait()
        if self.pg_started:
            self.pg_tool("pg_ctl", "-D", str(self.work / "pg"), "-m", "immediate",
                         "-w", "stop")
        if self.args.artifacts:
            self.args.artifacts.mkdir(parents=True, exist_ok=True)
            for path in self.work.iterdir():
                if path.is_file():
                    shutil.copy2(path, self.args.artifacts / path.name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--odyssey", type=Path, required=True)
    parser.add_argument("--pg-bin", type=Path, required=True)
    parser.add_argument("--entry", choices=("sql", "sighup"), nargs="+",
                        default=["sql", "sighup"])
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    args.odyssey = args.odyssey.resolve()
    with tempfile.TemporaryDirectory(prefix="group-reload-") as directory:
        test = Regression(args, Path(directory))
        try:
            test.start()
            for entry in args.entry:
                for case in CASES:
                    test.reject(entry, case)
                test.recover(entry)
            test.shutdown()
        except Exception:
            if test.log.exists():
                print(test.log.read_text(errors="replace")[-20000:], flush=True)
            for log in test.work.glob("*SAN_OPTIONS.*"):
                print(log.read_text(errors="replace"), flush=True)
            raise
        finally:
            test.cleanup()


if __name__ == "__main__":
    main()
