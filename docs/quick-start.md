# Quick start of Odyssey usage

## Local build

Currently Odyssey runs only on Linux. Supported platforms are x86/x86_64.

To make minimal build on ubuntu you will need:
```bash
sudo apt-get install openssl postgresql-server-dev-all build-essential cmake
```

After that you can simply do:
```bash
make build_release
```

This will create executable `odyssey` in `build/sources`, which can be run with config like:
```bash
build/sources/odyssey config.conf
```
Config can be as simple as:
```conf
daemonize yes

log_format "%p %t %l [%i %s] (%c) %m\n"
log_file "/var/log/odyssey.log"
log_to_stdout no
log_config yes
log_debug no
log_session yes
log_stats no
log_query no

coroutine_stack_size 24

listen {
	host "0.0.0.0"
	port 6432
}

storage "postgres_server" {
	type "remote"
	host "[127.0.0.1]:5432"
}

database "postgres" {
	user "postgres" {
		authentication "none"
		storage "postgres_server"
		pool "session"
	}
}
```