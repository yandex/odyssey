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
"allow"       - first try a non-SSL connection; if that fails, try an SSL connection
"prefer"      - first try an SSL connection; if that fails, try a non-SSL connection
"require"     - TLS required
"verify_ca"   - require valid certificate
"verify_full" - require valid certificate
```

The modes follow the `sslmode` parameter of libpq.

`allow` connects in plaintext first. A server that requires TLS refuses such a
connection only at startup, with `no pg_hba.conf entry ... no encryption`, and
only then does Odyssey drop the connection and retry it over TLS.
`backend_connect_timeout_ms` applies to each attempt separately.

The retry is not limited to the TLS-related error, since the error itself is not
a reliable signal: its text is translated according to the server `lc_messages`,
and a server that is not PostgreSQL reports the same situation differently. Like
libpq, Odyssey retries whenever the server answered the startup packet with an
ErrorResponse, whatever the error is; a connection that was closed or a reply
that could not be parsed is reported as is, without a second attempt.

Two exceptions are made:

* `57P03` (`cannot_connect_now`), just like libpq, which considers another host
  more promising than another encryption method
* `53300` (`too_many_connections`), `3D000` (`invalid_catalog_name`) and `28P01`
  (`invalid_password`) — unlike libpq. These are definite answers that TLS will
  not change, and repeating them for every client would only double the
  connection load on a server that is already refusing connections.

Note that with `allow` the startup packet and the authentication exchange of the
first attempt are sent in cleartext, and against a server that supports TLS but
does not require it the connection stays unencrypted. Use `prefer` to negotiate
TLS whenever the server supports it, or `require` to demand it.

Cancel requests send no startup packet and therefore have nothing to retry on,
so with `allow` they are always sent in plaintext. This does not make them fail:
a server handles a cancel request before authentication and without consulting
`pg_hba.conf`, so it accepts a plaintext one even when it otherwise allows
`hostssl` connections only. The cancel key does travel in cleartext then, just
as it does with the signal-safe `PQcancel()` of libpq. Use `prefer` or `require`
if that matters.

Connections preallocated for `min_pool_size` are established in plaintext as
well, since they are connected long before they are used. Such a connection is
retried over TLS when a client picks it up and the startup is finally
performed.

!!! warning
    Earlier versions of Odyssey negotiated TLS first for `allow` and fell back
    to plaintext, i.e. `allow` behaved the way `prefer` does now. Replace it
    with `prefer` to keep that behaviour — otherwise connections that used to be
    encrypted become plaintext. Odyssey logs a message at startup for every
    storage that still uses `allow`.

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
    method "roundrobin" {
    }
}
```

#### **az\_aware**
*yes|no*

Controls whether the balancing method takes the instance's availability zone
into account when selecting endpoints. Defaults to `yes`.

When `az_aware yes` (the default), Odyssey prefers endpoints located in the
same availability zone as the instance (as configured by `availability_zone`
in the [global section](../configuration/global.md)). Endpoints in other zones
are used only when no same-AZ endpoints are available.

Set to `no` to disable AZ-aware selection and treat all endpoints equally
regardless of their availability zone.

```plain
balancing {
    method "roundrobin" {
        az_aware no
    }
}
```

### **show\_notice\_messages**
*yes|no*

When set to `yes`, Odyssey sends PostgreSQL NOTICE messages to the client
when a balancing decision is made. Defaults to `no`.

```plain
balancing {
    method "roundrobin" {}
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

## Using storages in `listen` blocks

A `listen` block can reference one or more storages by name to enable
per-listen storage failover. See the [`storage` option in the listen
section](listen.md#storage) for details.

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
