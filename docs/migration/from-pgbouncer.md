# Migrating from PgBouncer

Odyssey can convert a supported subset of `pgbouncer.ini` into an Odyssey
configuration. Run the converter on a copy of your PgBouncer configuration,
review the result, and test it before moving application traffic.

```bash
umask 077
odyssey --convert-pgbouncer /etc/pgbouncer/pgbouncer.ini > odyssey.conf
odyssey --test odyssey.conf
```

The command writes only the generated configuration to standard output. For
input errors it reports the problem to standard error and exits without
writing a partial configuration. The generated file contains passwords from
PgBouncer's `auth_file`; keep its permissions restricted. A relative
`auth_file` path is resolved from the converter's working directory, as it is
by PgBouncer. Run the converter from the same directory used by the PgBouncer
service, or use an absolute path in a copy of the input configuration.

## What is converted

The converter accepts `[pgbouncer]` and `[databases]` with explicit TCP
addresses, plus the quoted `"user" "password"` entries in `auth_file`.
Each listed user gets an explicit Odyssey route for each database. For
example:

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

```text
"alice" "md500000000000000000000000000000000"
```

The generated route contains `database "app"`, `user "alice"`,
`authentication "md5"`, `pool "transaction"`, and `pool_size 20`.
It also creates a remote storage pointing to `127.0.0.1:5432`.

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

PgBouncer's defaults for client limit, Unix socket, pool mode, pool size,
listen port, backend port, and backend TLS are used when those settings are
absent. An explicit, single `listen_addr`,
`auth_file`, and TCP `host` for every database are required. Database
`pool_mode` and `pool_size` take precedence over global values.

When `unix_socket_dir` is absent, the converter assumes `/tmp` and mode
`0777`, the defaults in PgBouncer's upstream documentation. A packaged
PgBouncer build may use another socket directory. Check the generated socket
path and permissions, particularly when migrating `auth_type=trust`; set
`unix_socket_dir` explicitly in the source copy if needed.

The converter rejects settings outside this table, other sections such as
`[users]`, database wildcards, multiple hosts, Unix-socket backends, and
statement pooling. A database with a forced `user=` and more than one client
user is rejected, because that would split PgBouncer's single backend pool
into several Odyssey pools. It also rejects SCRAM secrets in `auth_file`:
those need a manual review of both client and backend authentication. Errors name the
unsupported setting so it can be migrated manually. The converter does not
silently replace unsupported behavior with a weaker setting.

## Check authentication and pooling

For `auth_type=md5`, PgBouncer accepts both clear-text and MD5 secrets in
`auth_file`. Odyssey can use either for client MD5 authentication. A stored
MD5 secret can be reused for backend MD5 authentication when the database
user is the same, but it cannot authenticate to a PostgreSQL server that
requires SCRAM. If the PgBouncer database entry forces another backend user,
the converter requires its `password=` value and writes it as
`storage_password`. Check the PostgreSQL authentication method and test a
real connection for every migrated route. Rotate any secret that was exposed
during migration.

`auth_type=trust` deliberately produces routes with no client password
check, restricted to names in `auth_file`. Review the listen address and
network controls before starting Odyssey. For other PgBouncer authentication
modes, configure [Odyssey authentication](../configuration/rules.md) by hand.

Odyssey supports session and transaction pooling. If your application uses
session state, temporary tables, or SQL-level prepared statements, verify
its behavior under transaction pooling before cutover. See
[pooling](../features/pooling.md) for the trade-offs.

## Cut over

1. Convert to a file with restricted permissions and run `odyssey --test`.
2. Review each route's backend address, database, user, password, pool size,
   and authentication method. Manually migrate rejected settings such as
   TLS, `auth_query`, admin access, and reset behavior.
3. Start Odyssey on a separate address or port. Test application login,
   representative transactions, prepared statements, and backend failover.
4. Compare active connections and latency under the expected load, then
   point clients to Odyssey. Keep the PgBouncer configuration available for
   rollback until the new path has been observed in production.

Odyssey's [per-route rules](../configuration/rules.md) let you set pooling
and authentication for each database/user pair. It also offers
[backend balancing](../features/balancing.md),
[Prometheus metrics](../features/prometheus-metrics.md), and
[online restart](../features/online-restart.md). These are useful reasons to
evaluate it after the basic migration; enable each feature separately once
the equivalent PgBouncer behavior is verified.

PgBouncer's [configuration reference](https://www.pgbouncer.org/config.html)
describes the source syntax and authentication behavior used by this guide.
