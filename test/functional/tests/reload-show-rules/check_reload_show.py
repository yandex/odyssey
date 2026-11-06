import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time

import psycopg


CONSOLE = "host=127.0.0.1 port=6432 user=postgres dbname=console connect_timeout=2"
BACKEND = "host=127.0.0.1 port=5432 user=postgres dbname=postgres connect_timeout=2"
GROUP = "host=127.0.0.1 port=6432 user=postgres dbname=group_db connect_timeout=2"
LOCK_KEY = 18001800
QUERY_MARKER = "reload-show-rules checker"


def connect(dsn):
    # The console supports the simple query protocol.
    return psycopg.connect(dsn, autocommit=True, prepare_threshold=None)


def wait_until(predicate, description, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.01)
    raise AssertionError(f"Timed out waiting for {description}")


class Query(threading.Thread):
    def __init__(self, connection, sql):
        super().__init__(daemon=True)
        self.connection = connection
        self.sql = sql
        self.error = None
        self.done = threading.Event()
        self.start()

    def run(self):
        try:
            self.connection.execute(self.sql)
        except Exception as error:
            self.error = error
        finally:
            self.done.set()

    def wait(self):
        assert self.done.wait(5), f"{self.sql} is blocked"
        if self.error is not None:
            raise self.error


def config(directory, version, with_unused=True):
    unused = ""
    if with_unused:
        unused = 'storage "unused" { type "remote" host "127.0.0.1" port 5433 }'
    return f"""
daemonize no
workers 4
log_to_stdout yes
log_format "%p %t %l %m\\n"
log_config no
log_session no
log_stats no
group_checker_interval 10ms
pid_file "{directory}/odyssey.pid"
locks_dir "{directory}"
unix_socket_dir "{directory}"
unix_socket_mode "0700"
listen {{ host "127.0.0.1" port 6432 }}
storage "local" {{ type "local" }}
storage "backend" {{ type "remote" host "127.0.0.1" port 5432 }}
{unused}
database "console" {{
    user default {{
        authentication "none"
        role "admin"
        storage "local"
        pool "session"
    }}
}}
database "postgres" {{
    user "postgres" {{
        authentication "none"
        storage "backend"
        pool "session"
        pool_routing "internal"
    }}
    user "reload_{version}" {{
        authentication "none"
        storage "backend"
        pool "session"
    }}
}}
database "group_db" {{
    group "members" {{
        authentication "none"
        storage "backend"
        storage_db "postgres"
        storage_user "postgres"
        pool "session"
        group_query "SELECT 'postgres'::text FROM pg_advisory_lock({LOCK_KEY}) /* {QUERY_MARKER} */"
        group_query_db "postgres"
        group_query_user "postgres"
    }}
}}
"""


def check(directory):
    config_path = directory / "config.conf"
    log_path = directory / "output.log"
    config_path.write_text(config(directory, 0))
    connections = []
    process = None
    stop = threading.Event()
    showers = []
    errors = []
    counts = [0, 0]
    completed = False

    with connect(BACKEND) as blocker:
        blocker.execute("SELECT pg_advisory_lock(%s)", (LOCK_KEY,))
        try:
            with log_path.open("w") as output:
                process = subprocess.Popen(
                    [os.environ.get("ODYSSEY_BIN", "/usr/bin/odyssey"), str(config_path)],
                    stdout=output,
                    stderr=subprocess.STDOUT,
                )

            def connect_console():
                try:
                    connections.append(connect(CONSOLE))
                    return True
                except psycopg.OperationalError:
                    assert process.poll() is None, log_path.read_text()
                    return False

            wait_until(connect_console, "console startup")
            connections.extend(connect(CONSOLE) for _ in range(3))
            reload_connection, rules_connection, storages_connection, probe = connections

            def checker_is_waiting():
                return blocker.execute(
                    "SELECT EXISTS (SELECT FROM pg_stat_activity "
                    "WHERE query LIKE %s AND wait_event = 'advisory' "
                    "AND clock_timestamp() - query_start < interval '200 milliseconds' "
                    "AND pid <> pg_backend_pid())",
                    (f"%{QUERY_MARKER}%",),
                ).fetchone()[0]

            wait_until(checker_is_waiting, "group checker blocked in its query")
            reload_query = Query(reload_connection, "RELOAD")
            wait_until(lambda: "importing changes" in log_path.read_text(), "reload to start")
            # Reload now waits for the parked checker under rules->mu. Each
            # SHOW used to take router->lock before waiting for rules->mu,
            # preventing the checker from completing once its query returned.
            rules_query = Query(rules_connection, "SHOW rules")
            storages_query = Query(storages_connection, "SHOW storages")
            time.sleep(0.05)
            assert not reload_query.done.is_set(), "checker was not parked during reload"
            blocker.execute("SELECT pg_advisory_unlock(%s)", (LOCK_KEY,))
            rules_query.wait()
            storages_query.wait()
            reload_query.wait()

            def show_loop(index, connection, sql):
                try:
                    while not stop.is_set():
                        connection.execute(sql).fetchall()
                        counts[index] += 1
                except Exception as error:
                    errors.append(error)

            for index, (connection, sql) in enumerate(
                [(rules_connection, "SHOW rules"), (storages_connection, "SHOW storages")]
            ):
                thread = threading.Thread(target=show_loop, args=(index, connection, sql), daemon=True)
                thread.start()
                showers.append(thread)

            # Remove a storage and repeatedly replace a rule while SHOW readers
            # and group membership updates run on other workers.
            for iteration in range(50):
                version = iteration % 2
                replacement = directory / "next.conf"
                replacement.write_text(config(directory, version, with_unused=False))
                replacement.replace(config_path)
                Query(reload_connection, "RELOAD").wait()
                rows = probe.execute("SHOW rules").fetchall()
                names = {row[1] for row in rows}
                assert f"reload_{version}" in names, rows
                assert f"reload_{1 - version}" not in names, rows
                rows = probe.execute("SHOW storages").fetchall()
                ports = {int(row[2]) for row in rows if row[1] == "127.0.0.1"}
                assert ports == {5432}, rows

            def group_is_usable():
                try:
                    with connect(GROUP) as member:
                        assert member.execute("SELECT 1").fetchone() == (1,)
                    return True
                except psycopg.OperationalError:
                    return False

            wait_until(group_is_usable, "group membership after reload")
            stop.set()
            for thread in showers:
                thread.join(5)
                assert not thread.is_alive(), "SHOW loop is blocked"
            assert not errors, errors
            assert all(counts), counts
            assert process.poll() is None, log_path.read_text()
            print(f"51 reloads, SHOW rules={counts[0]}, SHOW storages={counts[1]}, group route usable")
            completed = True
        finally:
            stop.set()
            if completed:
                for connection in connections:
                    connection.close()
            if process is not None:
                process.terminate()
                try:
                    process.wait(5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            for connection in connections:
                connection.close()
            if completed:
                assert process.returncode == 0, f"Odyssey exited with {process.returncode}"


with tempfile.TemporaryDirectory(prefix="odyssey-reload-show-rules-") as path:
    directory = Path(path)
    try:
        check(directory)
    except Exception:
        log = directory / "output.log"
        if log.exists():
            print(log.read_text())
        raise
