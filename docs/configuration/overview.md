# Configuration file overview

Typical Odyssey configuration file looks like:

```conf
daemonize yes

locks_dir "/tmp"

log_format "%p %t %l [%i %s] (%c) %m\n"
log_file "/var/log/odyssey.log"
log_to_stdout no
log_config yes
log_debug no
log_session yes
log_stats no

coroutine_stack_size 24

listen {
	host "127.0.0.1"
	port 6432
	tls "disable"
}

storage "postgres_server" {
	type "remote"
	host "127.0.0.1:5432"
}

storage "local" {
	type "local"
}

database "postgres" {
	user "postgres" {
		authentication "none"
		storage "postgres_server"
		pool "session"
	}
}

database "console" {
	user default {
		authentication "none"
		role "admin"
		pool "session"
		storage "local"
	}
}
```

It contains global section with common parameters like `log_debug`
and tuple of `database` sections which allows to specify pooling
settings for each pair of `[database, user]`.