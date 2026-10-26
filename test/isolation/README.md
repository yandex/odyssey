# Isolation tests

The native PostgreSQL `pg_isolation_regress` / `isolationtester` runner is
adapted from Odyssey PR #1706 (`c16c06f5`). Specs, expected output and the
schedule live in this directory. The suite runs through Odyssey as part of
`make regress_test`, including the existing CI build and compiler matrix.

`table_lock` uses two sessions taking conflicting `ACCESS EXCLUSIVE` table
locks. Both permutations require a waiting step to complete after the other
session commits.

## Lock detection through Odyssey

Odyssey's `BackendKeyData` contains a virtual PID. Odyssey recognizes the
native tester's `SELECT pg_catalog.pg_isolation_test_session_is_blocked($1,
'{...}')` using the full parser and translates both the bound victim PID and
the literal array of session PIDs to their current backend PIDs on each Bind.
The rewritten statement is sent to PostgreSQL, which checks the actual locks.
Sessions without a backend on the control connection's route and host are
omitted from the array; an unattached victim is translated to PID zero.

This requires `pool_reserve_prepared_statement yes` and query parsing enabled
(the default minimal mode or full mode). The PostgreSQL tester
is built without patches and keeps its normal application names.

## Running against an existing instance

Use the PostgreSQL tester built by `test/pg_regress/Dockerfile`:

```sh
PG_ISOLATION_REGRESS=/postgres/src/test/isolation/pg_isolation_regress \
PGHOST=127.0.0.1 PGPORT=6432 PGUSER=postgres PGDATABASE=postgres \
    test/isolation/run_tests.sh
```

Set `ISOLATION_OUTPUTDIR` to choose another output directory. The runner
returns `pg_isolation_regress`'s exit status and leaves its output and diffs
in that directory.
