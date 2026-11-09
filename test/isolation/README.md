# Isolation tests

The native PostgreSQL `pg_isolation_regress` / `isolationtester` runner is
adapted from Odyssey PR #1706 (`c16c06f5`). Specs, expected output and the
schedule live in this directory. The suite runs through Odyssey as part of
`make regress_test`, including the existing CI build and compiler matrix.

`table_lock` uses two sessions taking conflicting `ACCESS EXCLUSIVE` table
locks. Both permutations require a waiting step to complete after the other
session commits.

## Watchdog isolation tests

The regression Docker build enables `ENABLE_INJECTION_POINTS`. It stays OFF
by default in other builds, where console `LOAD` commands return an error.

`LOAD <point>` arms a point and the next call releases it. It can be armed
again once its waiters leave. `LOAD <point> WAIT` waits up to ten seconds
for a parked coroutine and errors if the point is not reached. The registry
is synchronized across workers; specs do not use sleeps or log polling.

`watchdog_reload` parks an old watchdog immediately before publishing a
primary's status, replaces that storage with a standby, and coordinates the
old watchdog's final poll, exit and join. After reload, a complete new poll
must publish read-only status for the replacement endpoint.

`watchdog_promote` promotes a real standby while its watchdog holds a
read-only result. It allows that old result to publish, then requires the
next complete poll to update the same cache entry to read-write. A client
requiring `target_session_attrs=read-write` must reach the promoted backend.

These schedules run at the end of `make regress_test`. The promotion uses
the replica on port 5435. The reload spec overwrites only the container's
`/home/postgres/regress/conf/isolation_watchdog_storage` fixture.

The patched tester's console mode (`ODYSSEY_ISOLATION_CONSOLE=1`) skips the
PostgreSQL-only setup and lock probe. Annotate asynchronous console steps
with `(*)`; unannotated steps wait for their result, making `LOAD ... WAIT`
a barrier. A next step on the same session joins an asynchronous command.
`ODYSSEY_ISOLATION_CONNINFO_<session>` overrides a named session's complete
libpq connection string so a spec can coordinate Odyssey and PostgreSQL.
The watchdog specs use direct `primary`/`standby` connections, and promotion
also uses a `client` connection through Odyssey. Nonzero `last_update_ms`
values in console results are printed as `<updated>`; zero stays visible,
so the expected output still requires a published status.

## Lock detection through Odyssey

Odyssey's `BackendKeyData` contains a virtual PID, so PostgreSQL's usual
isolation lock probe cannot use it as a backend PID. The Docker build applies
`isolationtester.patch`: each connection stores its virtual PID in
`application_name`, and the control connection resolves these names through
`pg_stat_activity` and checks actual blockers with `pg_blocking_pids()`.
Odyssey preserves `application_name` across transaction-pool assignments.
The probe only considers blockers belonging to this test's sessions.

Specs must preserve the runner's `application_name` and use a PostgreSQL user
that can inspect all test sessions. The regression container uses `postgres`.

## Running against an existing instance

Use the patched PostgreSQL tester built by `test/pg_regress/Dockerfile`:

```sh
PG_ISOLATION_REGRESS=/postgres/src/test/isolation/pg_isolation_regress \
PGHOST=127.0.0.1 PGPORT=6432 PGUSER=postgres PGDATABASE=postgres \
    test/isolation/run_tests.sh
```

Set `ISOLATION_OUTPUTDIR` to choose another output directory. The runner
returns `pg_isolation_regress`'s exit status and leaves its output and diffs
in that directory.
