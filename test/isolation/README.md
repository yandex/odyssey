# Isolation tests

The native PostgreSQL `pg_isolation_regress` / `isolationtester` runner is
adapted from Odyssey PR #1706 (`c16c06f5`). Specs, expected output and the
schedule live in this directory. The suite runs through Odyssey as part of
`make regress_test`, including the existing CI build and compiler matrix.

`table_lock` uses two sessions taking conflicting `ACCESS EXCLUSIVE` table
locks. Both permutations require a waiting step to complete after the other
session commits.

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
