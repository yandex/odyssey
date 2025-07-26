# Catchup timeout

The catchup timeout feature allows the pooler to automatically drop connections
to PostgreSQL hosts whose replication lag exceeds a specified threshold.
This ensures that clients do not interact with significantly out-of-date replicas,
helping maintain data freshness and consistency in read-scaling or high-availability
setups. Catchup timeout is especially valuable when application changes are difficult,providing a simple way to protect against stale reads by enforcing replication health
at the pooler level.

----

## Configuration

First of all, you will need to create watchdog in storage section.
It looks like this in your configuration file:
```plaintext
storage "postgres_server" {
	type "remote"
	host "localhost"
	port 5432

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
		pool_discard yes
		pool_cancel yes
		server_lifetime 1901
		log_debug no
		watchdog_lag_query "SELECT EXTRACT(EPOCH FROM ts) FROM repl_mon"
		watchdog_lag_interval 10
	}
}
```

The most intresting part is `watchdog_lag_query` parameter.
Watchdog will execute this query to get underlying server lag.
Consider something like `now() - pg_last_xact_replay_timestamp()` or
[repl_mon](https://github.com/man-brain/repl_mon) for production  usages

Also you need specify `cathcup_timeout` parameter in user section,
that sets maximum replication lag in seconds:
```plaintext
database "db" {
    user "user" {
        ...
        catchup_timeout 3
        catchup_checks 1
    }
}
```

See [storage configuration guide](../configuration/storage.md)
for more about storage section.