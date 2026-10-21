# tls_workers benchmark

Measures how client TLS handshakes affect already connected clients, with and
without `tls_workers`. Requires docker (postgres:16 with pgbench), python3 and
taskset.

```
# 16 persistent clients alone (A), with 32 plain reconnecting clients (B),
# with 32 TLS reconnecting clients (C)
./run.sh ../../build/sources/odyssey 3 1 reconnect

# 5 bursts of 100 clients connecting at once, with TLS and without it
./run.sh ../../build/sources/odyssey 3 1 burst tls
./run.sh ../../build/sources/odyssey 3 1 burst plain
```

Compare configurations with the same total number of threads
(`workers + tls_workers`), e.g. `4 0` with `3 1`.

Environment:

- `ODY_CPUS` (`0-3`), `PG_CPUS` (`4-6`), `CLIENT_CPU` (`7`): cpu layout, keep
  odyssey, postgres and the burst client on different cores
- `DUR` (`20`): seconds per phase in `reconnect` mode
- `WORK` (`./out`): config, certificate, logs and burst results

Postgres container `odyssey-bench-pg` is left running between runs, remove it
with `docker rm -f odyssey-bench-pg`. On a laptop run it under
`systemd-inhibit` so that suspend does not spoil the results.
