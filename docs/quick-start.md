# Quick start of Odyssey usage

There are several ways to start using Odyssey:

 - debian and ubuntu packages from PGDG
 - pre-built public docker image
 - local build of Docker image
 - local build

---
## Debian and Ubuntu packages

Odyssey is available via PGDG repository.

Learn more about the repository here: https://wiki.postgresql.org/wiki/Apt

To use the package, add PGDG:
```bash
$ sudo apt install -y postgresql-common ca-certificates
$ sudo /usr/share/postgresql-common/pgdg/apt.postgresql.org.sh
```

And then install Odyssey:
```bash
$ sudo apt-get install -y odyssey
```

The package will create simple config at `/etc/odyssey/odyssey.conf` and systemd service, that can be controlled with:
```bash
$ sudo systemctl start odyssey
$ sudo systemctl stop odyssey
$ sudo systemctl reload odyssey
$ sudo systemctl status odyssey
```

*Note: new releases can be available at repository after some time, day or two
after the release on Github.*

---
## Pre-built Docker image

The easiest way to get started with Odyssey is to use our pre-built Docker images from GitHub Container Registry.

Only version starts with `1.5.0` are supported.

### Quick start with default configuration

There are public images based on alpine on `ghcr.io/yandex/odyssey`.
You can try it with:

```bash
$ docker run -d \
   --name odyssey \
   -e PG_HOST=your-postgres-host \
   -e DB_NAME=mydb \
   -e USER_NAME=myuser \
   -e USER_AUTH_TYPE=none \
   --network host \
   ghcr.io/yandex/odyssey:1.5.0-rc2
```

Or with full config mount to `/etc/odyssey/odyssey.conf`:
```bash
$ docker run -d \
   --name odyssey \
   -v ./config.conf:/etc/odyssey/odyssey.conf \
   --network host \
   ghcr.io/yandex/odyssey:1.5.0-rc2
```

This will run Odyssey on port 6432, that is connected to `your-postgres-host`.
```bash
$ psql 'host=localhost port=6432 user=myuser dbname=mydb' -c 'select 42 as the_value'
 the_value 
-----------
        42
(1 row)
```

### Available image tags
 - latest - Latest stable release
 - edge - Latest development build from master branch
 - 1.5.0 - Specific stable version
 - 1.5.0-rc1 - Release candidate
 - 1.5 - Latest patch version in 1.5.x series
 - 1 - Latest minor version in 1.x series 

### Environment Variables Reference

#### Global Settings

| Variable | Default | Description |
|----------|---------|-------------|
| `DAEMONIZE` | `no` | Run as daemon (`yes`/`no`) |
| `SEQUENTIAL_ROUTING` | `no` | Enable sequential routing (`yes`/`no`) |
| `LOG_FORMAT` | `%p %t %l [%i %s] (%c) %m` | Log message format |
| `LOG_DEBUG` | `no` | Enable debug logging (`yes`/`no`) |
| `LOG_CONFIG` | `no` | Log configuration on startup (`yes`/`no`) |
| `LOG_SESSION` | `yes` | Log client sessions (`yes`/`no`) |
| `LOG_QUERY` | `no` | Log SQL queries (`yes`/`no`) |
| `LOG_STATS` | `yes` | Log statistics (`yes`/`no`) |
| `STATS_INTERVAL` | `10` | Statistics logging interval (seconds) |
| `WORKERS` | `"auto"` | Number of worker threads |
| `RESOLVERS` | `1` | Number of DNS resolver threads |
| `READAHEAD` | `4096` | Socket readahead buffer size (bytes) |
| `CACHE_COROUTINE` | `1024` | Coroutine cache size |
| `NODELAY` | `yes` | Enable TCP_NODELAY (`yes`/`no`) |
| `KEEPALIVE` | `15` | TCP keepalive interval (seconds) |
| `KEEPALIVE_KEEP_INTERVAL` | `5` | TCP keepalive probe interval (seconds) |
| `KEEPALIVE_PROBES` | `3` | Number of TCP keepalive probes |
| `KEEPALIVE_USR_TIMEOUT` | `0` | TCP user timeout (seconds, 0 = disabled) |
| `COROUTINE_STACK_SIZE` | `8` | Coroutine stack size (KB) |
| `CLIENT_MAX` | `20000` | Maximum client connections |
| `CLIENT_MAX_ROUTING` | `0` | Maximum routing operations per client (0 = unlimited) |
| `SERVER_LOGIN_RETRY` | `1` | Number of server login retry attempts |
| `HBA_FILE` | - | Path to HBA (host-based authentication) file |
| `GRACEFUL_DIE_ON_ERRORS` | `no` | Gracefully shutdown on errors (`yes`/`no`) |
| `GRACEFUL_SHUTDOWN_TIMEOUT_MS` | `30000` | Graceful shutdown timeout (milliseconds) |
| `AVAILABILITY_ZONE` | - | Availability zone identifier |
| `ENABLE_ONLINE_RESTART` | `yes` | Enable online restart support (`yes`/`no`) |
| `BINDWITH_REUSEPORT` | `yes` | Enable SO_REUSEPORT (`yes`/`no`) |
| `MAX_SIGTERMS_TO_DIE` | `3` | Number of SIGTERM signals before forced shutdown |
| `ENABLE_HOST_WATCHER` | `no` | Enable host watcher (`yes`/`no`) |

#### Listen Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `LISTEN_HOST` | `0.0.0.0` | Listen address (`*`, `0.0.0.0`, specific IP) |
| `LISTEN_PORT` | `6432` | Listen port |
| `BACKLOG` | `16` | TCP listen backlog size |
| `LISTEN_TLS_MODE` | `disable` | TLS mode (`disable`, `allow`, `require`, `verify_ca`, `verify_full`) |
| `LISTEN_TLS_CERT_FILE` | - | Path to TLS certificate file |
| `LISTEN_TLS_KEY_FILE` | - | Path to TLS private key file |
| `LISTEN_TLS_CA_FILE` | - | Path to TLS CA certificate file |
| `LISTEN_TLS_PROTOCOLS` | `tlsv1.2` | TLS protocol versions |
| `LISTEN_CLIENT_LOGIN_TIMEOUT` | `15000` | Client login timeout (milliseconds) |
| `LISTEN_TARGET_SESSION_ATTRS` | `any` | Target session attributes (`any`, `read-write`, `read-only`) |

#### Storage Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `STORAGE_NAME` | `postgres_server` | Storage name identifier |
| `PG_HOST` | `127.0.0.1` | PostgreSQL server host |
| `PG_PORT` | `5432` | PostgreSQL server port |
| `STORAGE_TLS_MODE` | `disable` | Storage TLS mode (`disable`, `allow`, `require`, `verify_ca`, `verify_full`) |
| `STORAGE_TLS_CERT_FILE` | - | Path to storage TLS certificate file |
| `STORAGE_TLS_KEY_FILE` | - | Path to storage TLS private key file |
| `STORAGE_TLS_CA_FILE` | - | Path to storage TLS CA certificate file |
| `STORAGE_TLS_PROTOCOLS` | `tlsv1.2` | Storage TLS protocol versions |
| `ENDPOINTS_STATUS_POLL_INTERVAL` | `1000` | Endpoints status polling interval (milliseconds) |
| `SERVER_MAX_ROUTING` | `0` | Maximum routing operations per server (0 = unlimited) |

#### Database and User Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `DB_NAME` | `default` | Database name (use `"dbname"` for quoted names) |
| `USER_NAME` | `default` | Database user name (use `"username"` for quoted names) |
| `USER_AUTH_TYPE` | `clear_text` | Authentication type (`none`, `clear_text`, `md5`, `scram-sha-256`, `cert`) |
| `USER_PASSWORD` | `password` | Database user password |
| `POOL_TYPE` | `session` | Connection pool type (`session` or `transaction`) |
| `POOL_SIZE` | `128` | Maximum pool size |
| `MIN_POOL_SIZE` | `0` | Minimum pool size (0 = disabled) |
| `POOL_TIMEOUT` | `0` | Pool timeout (milliseconds, 0 = unlimited) |
| `POOL_TTL` | `0` | Connection time-to-live (seconds, 0 = unlimited) |
| `POOL_DISCARD` | `no` | Discard connection after use (`yes`/`no`) |
| `POOL_SMART_DISCARD` | `no` | Smart discard based on connection state (`yes`/`no`) |
| `POOL_CANCEL` | `no` | Enable query cancellation (`yes`/`no`) |
| `POOL_ROLLBACK` | `yes` | Automatic rollback on connection return (`yes`/`no`) |
| `CLIENT_FWD_ERROR` | `yes` | Forward server errors to client (`yes`/`no`) |
| `APPLICATION_NAME_ADD_HOST` | `no` | Add client host to application_name (`yes`/`no`) |
| `RESERVE_SESSION_SERVER_CONNECTION` | `no` | Reserve server connection for session (`yes`/`no`) |
| `SERVER_LIFETIME` | `3600` | Server connection lifetime (seconds) |
| `POOL_CLIENT_IDLE_TIMEOUT` | `0` | Client idle timeout (milliseconds, 0 = unlimited) |
| `POOL_IDLE_IN_TRANSACTION_TIMEOUT` | `0` | Idle in transaction timeout (milliseconds, 0 = unlimited) |
| `POOL_RESERVE_PREPARED_STATEMENT` | `yes` | Reserve prepared statements (`yes`/`no`) |
| `USER_LOG_DEBUG` | `no` | Enable debug logging for this user (`yes`/`no`) |
| `MAINTAIN_PARAMS` | `no` | Maintain connection parameters (`yes`/`no`) |
| `USER_TARGET_SESSION_ATTRS` | `any` | User-level target session attributes |
| `QUANTILES` | - | Query latency quantiles to track (e.g., `"0.99"`) |

#### Virtual Database (Console) Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `VDB_STORAGE_NAME` | `local` | Virtual database storage name |
| `VIRTUAL_DB_NAME` | `"console"` | Virtual database name for admin console |
| `VIRTUAL_DB_USER_NAME` | `"console"` | Virtual database user name |
| `VIRTUAL_USER_AUTH_TYPE` | `none` | Virtual user authentication type |
| `VIRTUAL_USER_PASSWORD` | - | Virtual user password |

#### Advanced Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `PRE_INCLUDE_PATH` | - | Path to configuration file to include before main config |
| `POST_INCLUDE_PATH` | - | Path to configuration file to include after main config |
| `RUN_MODE` | `production` | Runtime mode (`production`, `test`, `debug`, `root`) |

#### Runtime Mode

The `RUN_MODE` variable controls how Odyssey runs:

- **`production`** (default): Runs as unprivileged `odyssey` user
- **`test`**: Runs as root (for testing with `/proc` access)
- **`debug`**: Runs as root with debug settings
- **`root`**: Runs as root

**Warning:** `test`, `debug`, and `root` modes should only be used in testing environments!

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

### Build with your config

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

Optional dependencies:
```bash
# For systemd notify support (online restart)
sudo apt-get install libsystemd-dev
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
