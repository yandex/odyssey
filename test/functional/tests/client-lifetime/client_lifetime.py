"""Check that attach failures and normal disconnects are counted once each."""

import csv
import io
import os
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile
import time


def wait_until(check, process, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError("Odyssey exited unexpectedly")
        result = check()
        if result is not None:
            return result
        time.sleep(0.05)
    raise AssertionError("Timed out waiting for Odyssey")


def main():
    binary = Path(sys.argv[1] if len(sys.argv) > 1 else "/usr/bin/odyssey").resolve()
    postgres_port = int(sys.argv[2]) if len(sys.argv) > 2 else 5432
    with tempfile.TemporaryDirectory(prefix="odyssey-client-lifetime-") as temp:
        root = Path(temp)
        with socket.socket() as reserved:
            reserved.bind(("127.0.0.1", 0))
            port = reserved.getsockname()[1]
        config = root / "odyssey.conf"
        config.write_text(f'''daemonize no
log_format "%p %t %l [%i %s] (%c) %m\\n"
log_to_stdout yes
log_session no
stats_interval 3600
workers 1
resolvers 1
enable_online_restart no
bindwith_reuseport no
locks_dir "{root}/locks"
listen {{ host "127.0.0.1" port {port} tls "disable" }}
storage "postgres" {{
    type "remote"
    host "127.0.0.1"
    port {postgres_port}
}}
storage "local" {{ type "local" }}
database "postgres" {{
    user "postgres" {{
        authentication "none"
        storage "postgres"
        pool "transaction"
        quantiles "0,0.5,1"
    }}
}}
database "console" {{
    user "console" {{
        authentication "none"
        role "admin"
        storage "local"
        pool "session"
        quantiles "0,0.5,1"
    }}
}}
''')
        env = os.environ.copy()
        env["PSQL_HISTORY"] = "/dev/null"

        def connection(database, user):
            return (f"host=127.0.0.1 port={port} dbname={database} "
                    f"user={user} sslmode=disable connect_timeout=3")

        def quantiles():
            result = subprocess.run(
                ["psql", connection("console", "console"), "-X", "--csv",
                 "-c", "SHOW POOLS_EXTENDED;"],
                env=env, capture_output=True, text=True, timeout=5)
            if result.returncode:
                return None
            for row in csv.DictReader(io.StringIO(result.stdout)):
                if row["database"] == "postgres" and row["user"] == "postgres":
                    return [int(row["client_lifetime_" + q]) for q in ("0", "0.5", "1")]
            return None

        log = (root / "odyssey.log").open("w+")
        server = subprocess.Popen([str(binary), str(config)], stdout=log, stderr=subprocess.STDOUT)
        normal = None
        try:
            def ready():
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                        return True
                except OSError:
                    return None

            wait_until(ready, server)
            failed = subprocess.run(
                ["psql", connection("postgres", "postgres") +
                 " options='-c odyssey.target_session_attrs=read-only'",
                 "-X", "-v", "ON_ERROR_STOP=1", "-c", "SELECT 1;"],
                env=env, capture_output=True, text=True, timeout=10)
            assert failed.returncode != 0, "Read-only attach must fail against the primary"
            assert "can't find suitable host for tsa 'read-only'" in failed.stderr, failed.stderr
            after_failure = wait_until(quantiles, server)
            assert after_failure[0] > 0, (
                f"Attach failure was not recorded: lifetime quantiles {after_failure}")
            assert after_failure[0] == after_failure[1] == after_failure[2], after_failure

            normal = subprocess.Popen(
                ["psql", connection("postgres", "postgres"), "-X", "-qAt",
                 "-v", "ON_ERROR_STOP=1"],
                env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, text=True)
            normal.stdin.write("SELECT 1;\n")
            normal.stdin.flush()
            assert select.select([normal.stdout], [], [], 5)[0], "Client did not become ready"
            assert normal.stdout.readline().strip() == "1"
            time.sleep(0.5)
            assert quantiles() == after_failure, "An open client must not add an observation"
            _, stderr = normal.communicate("\\q\n", timeout=5)
            assert normal.returncode == 0, stderr

            def both_disconnected():
                values = quantiles()
                return values if values and values[0] != values[2] else None

            minimum, median, maximum = wait_until(both_disconnected, server)
            assert after_failure[0] in (minimum, maximum), (minimum, maximum, after_failure)
            assert maximum >= 400_000, maximum
            # Two observations of weight one have their median halfway between
            # their endpoints. Counting the normal disconnect twice breaks this.
            assert abs(2 * median - minimum - maximum) <= 1, (
                f"Expected one observation per client, got {(minimum, median, maximum)}")
            print("Client lifetime: attach failure and normal disconnect counted once each")
        except BaseException:
            log.flush()
            print((root / "odyssey.log").read_text(), file=sys.stderr)
            raise
        finally:
            if normal is not None and normal.poll() is None:
                normal.kill()
                normal.communicate()
            if server.poll() is None:
                server.terminate()
                try:
                    server.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait(timeout=5)
            log.close()


if __name__ == "__main__":
    main()
