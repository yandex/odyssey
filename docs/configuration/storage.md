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

## **bindwith_reuseport**
*yes|no*

If specified, odyssey will bind socket with SO_REUSEPORT option.

## **tls**
*string*

Supported TLS modes:

```
"disable"     - disable TLS protocol
"allow"       - switch to TLS protocol on request
"require"     - TLS required
"verify_ca"   - require valid certificate
"verify_full" - require valid ceritifcate
```

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

## **watchdog**

Storage lag-polling watchdog

Defines storage lag-polling watchdog options and actually enables cron-like
watchdog for this storage. This routine will execute `watchdog_lag_query` against
storage server and send return value to all routes, to decide, if connecting is desirable 
with particular lag value.

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

    # Watchdog will execute this query to get underlying server lag. 
    # Consider something like now() - pg_last_xact_replay_timestamp() or 
    # git@github.com:man-brain/repl_mon.git for production  usages
    watchdog_lag_query "SELECT TRUNC(EXTRACT(EPOCH FROM NOW())) - 100"
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
