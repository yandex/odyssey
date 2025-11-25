# Global section

Parameters that sets Odyssey execution behaviour or that are common
for all Odyssey rules.

---

## Configuration Parameters Reference
| Parameter                                  | Type             | Default     | Reload  | Notes                                                 |
| ------------------------------------------ | ---------------- | ----------- | ------- | ----------------------------------------------------- |
| `include`                                  | string/list      | unset       | SIGHUP  | Include external config files (parsed, not in struct) |
| `daemonize`                                | int (bool)       | `no`        | restart | Prefer systemd over daemonize                         |
| `sequential_routing`                       | int (bool)       | `no`        | SIGHUP  | Match routes strictly in config order                 |
| `priority`                                 | int              | 0           | restart | Process priority (nice value)                         |
| `pid_file`                                 | string           | unset       | restart | PID file path                                         |
| `unix_socket_dir`                          | string           | unset       | restart | Enables UNIX socket comms                             |
| `unix_socket_mode`                         | string           | unset       | restart | Permissions for socket files (octal string)           |
| `locks_dir`                                | string           | unset       | restart | Location for lock files                               |
| `log_file`                                 | string           | unset       | SIGHUP  | Additional log output file                            |
| `log_format`                               | string           | unset       | SIGHUP  | Log message template                                  |
| `log_to_stdout`                            | int (bool)       | `yes`       | SIGHUP  | Logs to stdout                                        |
| `log_syslog`                               | int (bool)       | `no`        | SIGHUP  | Enable syslog output                                  |
| `log_debug`                                | int (bool)       | `no`        | SIGHUP  | Verbose debugging logs                                |
| `log_config`                               | int (bool)       | `no`        | SIGHUP  | Log config at start/reload                            |
| `log_session`                              | int (bool)       | `yes`       | SIGHUP  | Log client connect/disconnect                         |
| `log_query`                                | int (bool)       | `no`        | SIGHUP  | ⚠️ Logs client SQL queries                            |
| `log_stats`                                | int (bool)       | `yes`       | SIGHUP  | Log periodic route statistics                         |
| `promhttp_server_port`                     | int              | unset       | SIGHUP  | Enable Prometheus endpoint                            |
| `log_general_stats_prom`                   | int (bool)       | `no`        | SIGHUP  | Prometheus general stats                              |
| `log_route_stats_prom`                     | int (bool)       | `no`        | SIGHUP  | Prometheus per-route stats                            |
| `stats_interval`                           | int (sec)        | `3`         | SIGHUP  | Interval for stats logging                            |
| `workers`                                  | int              | `1`         | restart | Worker threads for clients                            |
| `resolvers`                                | int              | `1`         | restart | DNS resolver threads                                  |
| `readahead`                                | int (bytes)      | `8192`      | SIGHUP  | Per-connection read buffer                            |
| `cache_coroutine`                          | int              | `0`         | restart | Coroutine cache size                                  |
| `nodelay`                                  | int (bool)       | `yes`       | SIGHUP  | Enable TCP\_NODELAY                                   |
| `keepalive`                                | int (sec)        | `15`        | SIGHUP  | TCP keepalive; 0 disables                             |
| `keepalive_keep_interval`                  | int (sec)        | `5`         | SIGHUP  | Interval between probes                               |
| `keepalive_probes`                         | int              | `3`         | SIGHUP  | Probes before killing conn                            |
| `keepalive_usr_timeout`                    | int (ms)         | `0`         | SIGHUP  | 0 = use system default (`TCP_USER_TIMEOUT`)           |
| `backend_connect_timeout_ms`               | int (ms)         | `30000`     | SIGHUP  | Backend connection timeout                            |
| `coroutine_stack_size`                     | int (pages)      | `4`         | restart | Coroutine stack size                                  |
| `client_max`                               | int              | `0`         | SIGHUP  | Max client connections (0/unset = no global limit)    |
| `client_max_routing`                       | int              | `0`         | SIGHUP  | 0/unset → auto (typically `64 * workers`)             |
| `server_login_retry`                       | int              | `1`         | SIGHUP  | Retry delay on "Too many clients"                     |
| `hba_file`                                 | string           | unset       | SIGHUP  | Path to pg\_hba-like rules                            |
| `graceful_die_on_errors`                   | int (bool)       | `no`        | runtime | Shutdown on SIGUSR2 (stop accepting new, keep old)    |
| `graceful_shutdown_timeout_ms`             | int (ms)         | `30000`     | runtime | Graceful shutdown timeout                             |
| `availability_zone`                        | string           | unset       | restart | Used for host selection                               |
| `enable_online_restart`                    | int (bool)       | `no`        | restart | Allow zero-downtime restart                           |
| `online_restart_drop_options.drop_enabled` | int (bool)       | `yes`       | runtime | Drop old connections gradually                        |
| `bindwith_reuseport`                       | int (bool)       | `no`        | restart | Use SO\_REUSEPORT for binding                         |
| `max_sigterms_to_die`                      | int              | `3`         | SIGHUP  | Max SIGTERMs before hard exit                         |
| `enable_host_watcher`                      | int(bool)        | `3`         | restart | Start host cpu and mem consumption watcher thread      |


## **include**
*string*

Include one or more configuration files. Include files can
include other files.

`include "path"`

## **daemonize**
*yes|no*

Start as a daemon.

By default Odyssey does not run as a daemon. Set to 'yes' to enable.

`daemonize no`

## **sequential\_routing**
*yes|no*

Try to match routes exactly in config order.

By default, Odyssey tries to match all specific routes first, and then all default ones.
It may be confusing because auth-denying default route can be overridden with more specific auth-permitting route below in the config.
With this option set, Odyssey will match routes exactly in config order, like in HBA files.

`sequential_routing no`

## **priority**
*integer*

Process priority.

Set Odyssey parent process and threads priority.

`priority -10`

## **pid\_file**
*string*

If pid\_file is specified, Odyssey will write its process id to
the specified file at startup.

`pid_file "/var/run/odyssey.pid"`

## **unix\_socket\_dir**
*string*

UNIX socket directory.

If `unix_socket_dir` is specified, Odyssey will enable UNIX socket
communications. Specified directory path will be used for
searching socket files.

`unix_socket_dir "/tmp"`

## **unix\_socket\_mode**
*string*

Set `unix_socket_mode` file mode to any created unix files.

`unix_socket_mode "0755"`

## **locks_dir**
*string*

If `locks_dir` is specified, directory path will be used for 
placing lock files.

## **log\_file**
*string*

If log\_file is specified, Odyssey will additionally use it to write
log events.

`log_file "/var/log/odyssey.log"`

## **log\_format**
*string*

Log text format.

Odyssey allows to configure log text format. This could be useful to
support external log parser format. Format string can contain plain
text, escape symbols and format flags.

Supported flags:

```
%n = unixtime
%t = timestamp with date in iso 8601 format
%e = millisEcond
%p = process ID
%i = client ID
%s = server ID
%u = user name
%d = database name
%c = context
%l = level (error, warning, debug)
%m = message
%M = message tskv
%r = client port
%h = client host
%H = server host
```

`log_format "%p %t %e %l [%i %s] (%c) %m\n"`

## **log\_to\_stdout**
*yes|no*

Set to 'yes' if you need to additionally display log output in stdout.
Enabled by default.

`log_to_stdout yes`

## **log\_syslog**
*yes|no*

Log to system logger.

To enable syslog(3) usage, set log\_syslog to 'yes'. Additionally set
log\_syslog\_ident and log\_syslog\_facility.

`log_syslog no`

## **log\_syslog\_ident**
*string*

Set syslog ident name.

`log_syslog_ident "odyssey"`

## **log\_syslog\_facility**
*string*

Set syslog facility name.

`log_syslog_facility "daemon"`

## **log\_debug**
*yes|no*

Enable verbose logging of all events, which will generate a log of
detailed information useful for development or testing.

It is also possible to enable verbose logging for specific users
(see routes section).

`log_debug no`

## **log\_config**
*yes|no*

Write configuration to the log during start and config reload.

`log_config yes`

## **log\_session**
*yes|no*

Write client connect and disconnect events to the log.

`log_session yes`

## **log\_query**
*yes|no*

Write client queries text to the log. Disabled by default.

`log_query no`

## **log\_stats**
*yes|no*

Periodically display information about active routes.

`log_stats yes`

## **promhttp_server_port**
*integer*

Port on which metrics server listen. *http://localhost:port/* -- check is port running. *http://localhost:port/metrics* -- get metrics as a response.

## **log\_general\_stats_prom**
*yes|no*

Write information about active routes in Prometheus format in addition to ordinary format. Requires [C Prometheus client library](https://github.com/digitalocean/prometheus-client-c) installed. Log only info not specific to route

## **log\_route\_stats_prom**
*yes|no*

Write information about active routes in Prometheus format in addition to ordinary format. Requires [C Prometheus client library](https://github.com/digitalocean/prometheus-client-c) installed. Log all available info

## **stats\_interval**
*integer*

Set interval in seconds for internal statistics update and log report.

`stats_interval 3`

## **workers**
*integer*

Set size of thread pool used for client processing.

1: By default, Odyssey runs with a single worker. This is a special
mode optimized for general use. This mode also made to reduce multi-thread
communication overhead.

N: Add additional worker threads, if your server experience heavy load,
especially using TLS setup.

`workers 1`

## **resolvers**
*integer*

Number of threads used for DNS resolving. This value can be increased, if
your server experience a big number of connecting clients.

`resolvers 1`

## **readahead**
*integer*

Set size of per-connection buffer used for io readahead operations.

`readahead 8192`

## **cache\_coroutine**
*integer*

Set pool size of free coroutines cache. It is a good idea to set
this value to a sum of max clients plus server connections. Please note, that
each coroutine consumes around 16KB of memory.

Set to zero, to disable coroutine cache.

`cache_coroutine 128`

## **nodelay**
*yes|no*

TCP nodelay. Set to 'yes', to enable nodelay.

`nodelay yes`

## **keepalive**
*integer*

TCP keepalive time. Set to zero, to disable keepalive.

`keepalive 15`

## **keepalive_keep_interval**
*integer*

The number of seconds between TCP keep-alive probes.
5 by default.

`keepalive_keep_interval 10`

## **keepalive_probes**
*integer*

TCP keep-alive probes to send before  giving  up  and  killing  the connection if no response is obtained.
3 by default.

`keepalive_probes 5`

## **keepalive_usr_timeout**
*integer*

When the value is greater than 0, it specifies the maximum amount of time in milliseconds that transmitted data may remain unacknowledged before TCP will forcibly close the
corresponding connection.

When the value is negative, the default system user timeout will be used.

When no value or 0 is provided `1000 * (keepalive + keepalive_keep_interval * keepalive_probes) - 500` is used.

`keepalive_usr_timeout 7`

## **backend_connect_timeout_ms**
*integer*

Timeout for connection to backend (postgres). Default value is 30000 (30 secs)

`backend_connect_timeout_ms 20000`

## **coroutine\_stack\_size**
*integer*

Coroutine stack size.

Set coroutine stack size in pages. In some rare cases
it might be necessary to make stack size bigger (like that using the Odyssey with LDAP auth required `coroutine_stack_size 16`). Actual stack will be
allocated as `(coroutine_stack_size + 1_guard_page) * page_size`.
Guard page is used to track stack overflows. Stack by default is set to 16KB.

`coroutine_stack_size 4`

## **client\_max**
*integer*

Global limit of client connections.

Comment 'client_max' to disable the limit. On client limit reach, Odyssey will
reply with 'too many connections'.

`client_max 100`

## **client_max_routing**
*integer*

Global limit of client connections concurrently being routed.
Client connection is being routed after it is accepted and until it's startup
message is read and connection is assigned route to the database. Most of the
routing time is occupied with TLS handshake.
Unset or zero 'client_max_routing' will set it's value equal to 64 * workers

`client_max_routing 32`

## **server_login_retry**
*integer*

If server responds with "Too many clients" client will wait for server_login_retry milliseconds.
1 by default.

## **hba\_file**
*string*

Path to file containing host based authentication rules.
Omit this option to disable HBA.

`hba_file "path"`

HBA file format follows the format of the PostgreSQL `pg_hba.conf` file.
* Supported record types: `local`, `host`, `hostssl`, `hostnossl`.
* Database field: `all`, `sameuser`, multiple names.
* User field: `all`, multiple names.
* Address field: IPv4 or IPv6 range.
* Auth-method field: `deny` or `reject` (equivalent keywords), which leads to immediate disconnection,
`allow` or `trust` (also equivalent keywords), which means applying auth method specified in matching route.

## **graceful_die_on_errors**
*yes|no*

If specified, after receiving the signal SIGUSR2, 
Odyssey will shutdown the socket for receptions and continue working only with old connections

## **graceful_shutdown_timeout_ms**
*integer*

Timeout for graceful shutdown in milliseconds
Default:30000 (30 seconds), use 0 to disable

`graceful_shutdown_timeout_ms 30000`

## **availability_zone**
*string*

Specify Odyssey instance availability zone for host selection.

`availability_zone "us-east-1a"`

## **enable_online_restart**
*yes|no*

Online restart feature.
When setting to yes, restart odyssey simply with 
running new version (old one will automatically perform graceful shutdown)

`enable_online_restart no`

## **online_restart_drop_options**

This section can be used to configure connections dropping during
online restart

### **drop_enabled**
*yes|no*

When set to yes - connections to old odyssey instance
will be dropped by rate of one per sec for each worker.
When set to no - odyssey will work with connections
on old instance until it disconnect.
Default: yes

```plain
online_restart_drop_options {
	drop_enabled no
}
```

## **bindwith_reuseport**
*yes/no*

## **max_sigterms_to_die**
*integer*

Maximum SIGTERM count before hard exit(1)

`max_sigterms_to_die 10`

## **enable_host_watcher**
*yes/no*

Start thread to watch host CPU and memory consumption. Makes `show host_utilization` works.

`enable_host_watcher yes`

