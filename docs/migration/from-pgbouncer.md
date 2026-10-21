# Migrating from PgBouncer

Odyssey converts supported settings from `pgbouncer.ini` and warns about
settings that need manual migration. Run the converter on a copy of your
PgBouncer configuration, then review and test the result before moving
application traffic.

Odyssey combines multicore pooling with backend routing in a single process.
Its worker threads share backend connection pools and handle TLS processing,
so deployments that run
[multiple PgBouncer processes](https://www.pgbouncer.org/config.html#so_reuseport)
to use more cores can become simpler to operate.
[Per-route rules](../configuration/rules.md) give you control over individual workloads,
while [backend balancing](../features/balancing.md) and
[target session attributes](../features/tsa.md) select appropriate PostgreSQL
backends. [Online restart](../features/online-restart.md) lets you upgrade the
binary while the old instance drains existing connections, and
[Prometheus metrics](../features/prometheus-metrics.md) make queues, pool
saturation, and connection errors visible.

```bash
umask 077
odyssey --convert-pgbouncer /etc/pgbouncer/pgbouncer.ini > odyssey.conf
odyssey --test odyssey.conf
```

The command writes the configuration to standard output. It adds
`# WARNING:` comments for settings that need manual migration and prints the
same warnings to standard error. Warnings do not change the exit status of
0; read them before using the result. Invalid syntax, duplicate supported
settings, invalid numeric values, and file read errors cause a nonzero exit
status and no configuration output.

The generated file contains passwords from PgBouncer's `auth_file`. Keep its
permissions restricted. As in PgBouncer, a relative `auth_file` path is
resolved from the process's working directory. Run the converter from the
PgBouncer service's working directory, or use an absolute path in your copy
of the input configuration.

## What is converted

The converter reads `[pgbouncer]` and `[databases]` with explicit TCP
addresses, and the quoted `"user" "password"` entries in `auth_file`.
It creates an Odyssey route for each database and listed user. For example:

`pgbouncer.ini`:

```ini
[pgbouncer]
listen_addr = 127.0.0.1
listen_port = 6432
auth_type = md5
auth_file = userlist.txt
pool_mode = transaction
default_pool_size = 20

[databases]
app = host=127.0.0.1 port=5432 dbname=app
```

`userlist.txt`:

```text
"alice" "md500000000000000000000000000000000"
```

The resulting `odyssey.conf`:

```conf
# Generated from PgBouncer. Review credentials and limits before use.
# This file contains auth_file secrets; restrict its permissions.
log_format "%p %t %l [%i %s] (%c) %m\n"
unix_socket_dir "/tmp"
unix_socket_mode "0777"
client_max 100

listen {
    host "127.0.0.1"
    port 6432
}

listen {
    port 6432
}

storage "pgb_1" {
    type "remote"
    host "127.0.0.1"
    port 5432
    tls "prefer"
}

database "app" {
    user "alice" {
        authentication "md5"
        password "md500000000000000000000000000000000"
        storage "pgb_1"
        storage_db "app"
        pool "transaction"
        pool_size 20
        min_pool_size 0
    }
}
```

| PgBouncer | Odyssey |
| --- | --- |
| `listen_addr`, `listen_port` | `listen { host, port }` |
| `unix_socket_dir`, `unix_socket_mode` | Global socket settings and a Unix socket listener |
| `max_client_conn` | Global `client_max` |
| `logfile`, `pidfile` | Global `log_file`, `pid_file` |
| Database `host`, `port` | A remote `storage` per database |
| Default backend TLS mode `prefer` | Storage `tls "prefer"` |
| Database `dbname`, `user`, `password` | Route `storage_db`, `storage_user`, `storage_password` |
| Global or database `pool_mode` | Route `pool` (`session` or `transaction`) |
| `default_pool_size`, database `pool_size` | Route `pool_size` |
| Global or database `min_pool_size` | Route `min_pool_size` |
| `auth_type=md5` and `auth_file` | Named routes with `authentication "md5"` and `password` |
| `auth_type=trust` and `auth_file` | Named routes with `authentication "none"`; the listed password remains available for backend login |

For omitted settings, the converter uses PgBouncer's defaults for the client
limit, Unix socket, pool mode and size, listen and backend ports, and backend
TLS. To convert all routes, specify one `listen_addr`, an `auth_file`, and a
TCP `host` for every database. Database `pool_mode` and `pool_size` override
the global values.

If `unix_socket_dir` is absent, the converter uses `/tmp` and mode `0777`,
the defaults in PgBouncer's upstream documentation. Your PgBouncer package
may use another socket directory. Check the generated path and permissions,
especially for `auth_type=trust`. Set `unix_socket_dir` in the source copy
if needed.

The converter skips settings outside this table, other sections such as
`[users]`, and `%include` directives, with a warning for each. Expand included
files before conversion. Skipping an include blocks all routes because its
settings are unknown. Skipping a `[users]` section also blocks all routes
because its pool overrides have not been converted. Warnings about skipped
options give the key and its location, but omit the value, which may contain
credentials.

When a route cannot be converted, the converter writes it with
`authentication "block"` so you can finish it by hand. This applies to
unsupported authentication or TLS settings, SCRAM secrets, statement pooling,
database wildcards, and backend addresses without a single TCP host. Skipping
`connect_query` blocks the affected database: the query may set the backend
role or required session state. A forced `user=` with multiple client users
also blocks the database, since separate Odyssey pools would change the
connection limit of PgBouncer's single backend pool. Blocked routes use
`min_pool_size 0` to prevent backend preallocation.

Missing credentials leave a blocked `user default` placeholder. Unsupported
backend addresses leave a local storage placeholder; replace it with a remote
storage before enabling the route. An unsupported or missing `listen_addr`
leaves a loopback placeholder and blocks all routes.

## Check authentication and pooling

With `auth_type=md5`, PgBouncer accepts clear-text passwords and MD5 secrets
in `auth_file`. Odyssey can use either for client MD5 authentication. A stored
MD5 secret also works for backend MD5 authentication if the database user is
the same. It cannot authenticate to a PostgreSQL server that requires SCRAM.
For a database entry that forces another backend user, the converter writes
`password=` as `storage_password`. If that password is missing, it blocks the
route and prints a warning. Check PostgreSQL's authentication method and test
a real connection for every migrated route. Rotate any secret exposed during
migration.

`auth_type=trust` produces routes with no client password check, restricted
to names in `auth_file`. Review the listen address and
network controls before starting Odyssey. For other PgBouncer authentication
modes, configure [Odyssey authentication](../configuration/rules.md) by hand.

Odyssey supports session and transaction pooling. If your application uses
session state, temporary tables, or SQL-level prepared statements, verify
its behavior under transaction pooling before cutover. See
[pooling](../features/pooling.md) for the trade-offs.

## Cut over

1. Convert to a file with restricted permissions, resolve its warnings, and
   run `odyssey --test`.
2. Review each route's backend address, database, user, password, pool size,
   and authentication method. Manually migrate warned settings such as
   TLS, `auth_query`, admin access, and reset behavior.
3. Start Odyssey on a separate address or port. Test application login and
   representative transactions, including prepared statements. Check backend
   failover too.
4. Compare active connections and latency under the expected load before
   pointing clients to Odyssey. Keep the PgBouncer configuration for rollback
   while you observe Odyssey in production.

PgBouncer's [configuration reference](https://www.pgbouncer.org/config.html)
describes the source syntax and authentication behavior used by this guide.
