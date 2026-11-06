# PgBouncer converter tests

Build Odyssey, then run from the repository root:

```sh
bash test/pgbouncer-converter/runner.sh
```

Set `ODYSSEY_BIN` to use another build. Pass case directory names to run a
subset, for example `bash test/pgbouncer-converter/runner.sh tls-both-disabled`.
The runner uses Bash and standard shell utilities; Python is not required.

Each directory under `cases/` contains a `pgbouncer.ini`, any required
`userlist.txt`, and the expected `expected.conf` and/or `expected.stderr`.
An absent `expected.conf` means conversion must fail with exit code 1 and
empty stdout. An absent `expected.stderr` means stderr must be empty.
Expected files are checked in and compared in full with `diff`; the runner
does not regenerate them. Every successful result also passes `odyssey --test`.

Cases run in separate temporary directories. The relative-auth-file case
places its INI under `source/` to check that `auth_file` resolves from the
working directory. The empty-logfile case has a `startup` marker: the runner
replaces `@PORT@` in its input and expected configuration with a random port,
starts Odyssey, and checks its Unix socket and a TCP connection. Processes and temporary files
are cleaned up on exit. The startup check needs permission to bind local
sockets; no PostgreSQL server is needed.
