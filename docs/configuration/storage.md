# Storage section

Defines server used as a data storage or admin console operations.

`storage <name> { options }`

---

## **type**
*string*

Set storage type to use. Supported types:

```
"remote" - PostgreSQL server
"local"  - Odyssey (admin console)
```

`type "remote"`

Local console supports RELOAD, SHOW and KILL_CLIENT commands.

## **host**
*string*

Remote server address.

If host is not set, Odyssey will try to connect using UNIX socket if
`unix_socket_dir` is set.

## **port**
*integer*

Remote server port.

## **tls**
*string*

Supported TLS modes:

```
"disable"     - disable TLS protocol
"allow"       - switch to TLS protocol on request
"require"     - TLS required
"verify_ca"   - require valid certificate
"verify_full" - require valid certificate
```

## **tls\_ca\_file**
*string*

Path to CA certificate file used to verify the server certificate (for
`verify_ca` / `verify_full` modes).

`tls_ca_file "/etc/odyssey/ssl/allCAs.pem"`

## **tls\_key\_file**
*string*

Path to client private key file used for mutual TLS authentication.

`tls_key_file "/etc/odyssey/ssl/client.key"`

## **tls\_cert\_file**
*string*

Path to client certificate file used for mutual TLS authentication.

`tls_cert_file "/etc/odyssey/ssl/client.crt"`

## **tls\_protocols**
*string*

Allowed TLS protocol versions.

`tls_protocols "tlsv1.2"`

## **endpoints_status_poll_interval**
*integer*

If target_session_attrs is set (from listen or query), Odyssey will check every endpoint attrs
not more often than endpoints_status_poll_interval milliseconds within one conn
Default 1000

`endpoints_status_poll_interval 1000`

## **server_max_routing**
*integer*

Global limit of server connections concurrently being routed.
We are opening no more than server_max_routing server connections concurrently.
Unset or zero 'server_max_routing' will set it's value equal to number of workers

`server_max_routing 4`

## **balancing**

Optional sub-section that configures load-balancing behaviour for this storage.
See [balancing](../features/balancing.md) for a full overview.

### **method**
*string*

Balancing method name. Defines how Odyssey picks among the storage endpoints.

```plain
balancing {
    method "round-robin" {
    }
}
```

### **show\_notice\_messages**
*yes|no*

When set to `yes`, Odyssey sends PostgreSQL NOTICE messages to the client
when a balancing decision is made. Defaults to `no`.

```plain
balancing {
    method "round-robin" {}
    show_notice_messages yes
}
```

## **watchdog**

Storage lag-polling watchdog.

Defines storage lag-polling watchdog options and actually enables a cron-like
watchdog for this storage. The watchdog periodically executes `watchdog_lag_query`
against the storage server, then uses the result to decide whether connecting to
that host is desirable given the current replication lag.

### How the query result is interpreted

The query **must return a single integer** — the Unix timestamp (seconds since
epoch) of the last WAL data replayed on this replica, i.e. the point in time
*up to which* the replica has replayed the WAL. Odyssey computes the lag as:

```
lag_sec = current_unix_time - query_result
```

This is **not** a lag value in seconds — it is the replay timestamp itself.
Odyssey does the subtraction internally.

A convenient way to obtain this value is
[repl_mon](https://github.com/man-brain/repl_mon), a PostgreSQL bgworker that
writes `current_timestamp` (on the primary) and the current WAL LSN to a small
table once per second. On a replica the query reads back that timestamp:

```sql
SELECT TRUNC(EXTRACT(EPOCH FROM ts))::int FROM repl_mon LIMIT 1
```

For a quick test without repl_mon you can use a synthetic expression that
returns a fixed timestamp a few seconds in the past:

```sql
SELECT TRUNC(EXTRACT(EPOCH FROM NOW()))::int - 100
```

This simulates a replica that is always 100 seconds behind.

### watchdog\_lag\_query

*string*

SQL query executed periodically by the watchdog. Must return a single integer:
the Unix timestamp (seconds) up to which the replica has replayed WAL.

`watchdog_lag_query "SELECT TRUNC(EXTRACT(EPOCH FROM ts))::int FROM repl_mon LIMIT 1"`

### watchdog\_lag\_interval

*integer*

Interval in seconds between successive executions of `watchdog_lag_query`.

`watchdog_lag_interval 10`

```plain
watchdog {
    authentication "none"

    storage "postgres_server"
    storage_db "postgres"
    storage_user "postgres"

    pool_routing "internal"
    pool "transaction"
    pool_size 10

    pool_timeout 0
    pool_ttl 1201

    log_debug no

    # Query must return the Unix timestamp up to which the replica has
    # replayed WAL.  Odyssey computes: lag_sec = now() - query_result.
    # repl_mon (https://github.com/man-brain/repl_mon) writes this value
    # from the primary once per second; read it back on the replica:
    watchdog_lag_query "SELECT TRUNC(EXTRACT(EPOCH FROM ts))::int FROM repl_mon LIMIT 1"
    watchdog_lag_interval 10
}
```

## example

```
storage "postgres_server" {
	type "remote"
	host "127.0.0.1"
	port 5432
#	tls "disable"
#	tls_ca_file ""
#	tls_key_file ""
#	tls_cert_file ""
#	tls_protocols ""
}
```
