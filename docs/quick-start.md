# Quick start of Odyssey usage

---
## Docker build

After that you can simply do:
```bash
make quickstart
```

This will create and run `odyssey` as `odyssey`, with base [config](../docker/quickstart/config.conf).

This configuration writes logs to the stdout, so to view the logs, you can do:
```bash
docker logs odyssey
```

### Bild with your config

You can start odyssey with your config. You need to do:
```bash
docker build -f docker/quickstart/Dockerfile . --tag=odyssey:alpine
```

Then just replace PATH_TO_YOUR_CONFIG and do this command:
```bash
docker run -d \
		--rm \
		--name "odyssey" \
	 	-v {PATH_TO_YOUR_CONFIG}:/etc/odyssey/odyssey.conf \
		odyssey:alpine
```

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