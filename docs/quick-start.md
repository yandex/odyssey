# Quick start of Odyssey usage

---
## Docker build

After that you can simply do:
```bash
make quickstart
```

This will create and run `odyssey` as `odyssey`, with base config from `docker/quickstart/config.conf`.

This configuration writes logs to the stdout, so to view the logs, you can do:
```bash
docker logs odyssey
```

### Bild with your config

You can start odyssey with your config. You need to do:
```bash
docker build -f docker/quickstart/Dockerfile . --tag=odyssey
```

There are two ways to achieve this. The first method is to modify the default configuration. Simply add new parameters to the "docker run" command:
```bash
docker run -e LISTEN_PORT=1234 \
		--rm \
		--name "odyssey" \
		odyssey
```

You can find other settings with default values that you can modify below:
```bash
LOS_SESSION=yes
LOS_QUERY=no
LISTEN_HOST="*"
LISTEN_PORT=6432
PG_HOST="127.0.0.1"
PG_PORT=5432
DB_NAME=default
USER_NAME=default
USER_AUTH_TYPE="clear_text"
USER_PASSWORD="password"
POOL_TYPE="session"
POOL_SIZE=1
VIRTUAL_DB_NAME="console"
VIRTUAL_DB_USER_NAME="console"
```

Another way is to use volume to start with your own config file. Simply replace PATH_TO_YOUR_CONFIG with the path to your configuration file and run this command:
```bash
docker run -d \
		--rm \
		--name "odyssey" \
	 	-v PATH_TO_YOUR_CONFIG:/etc/odyssey/odyssey.conf \
		odyssey
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