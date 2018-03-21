
## Odyssey configuration file reference.

#### include "string"

Include one or more configuration files. Include files can
include other files.

`include "path"`

### Service control

#### daemonize yes/no

Start as a daemon.

By default Odyssey does not run as a daemon. Set to 'yes' to enable.

`daemonize no`

#### pid\_file "string"

If pid\_file is specified, Odyssey will write its process id to
the specified file at startup.

`pid_file "/var/run/odyssey.pid"`

### Logging

#### log\_file "string"

If log\_file is specified, Odyssey will additionally use it to write
log events.

`log_file "/var/log/odyssey.log"`

#### log\_format "string"

Log text format.

Odyssey allows to configure log text format. This could be useful to
support external log parser format. Format string can contain plain
text, escape symbols and format flags.

Supported flags:

```
%n = unixtime
%t = timestamp with date
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
```

`log_format "%p %t %l [%i %s] (%c) %m\n"`

#### log\_to\_stdout yes/no

Set to 'yes' if you need to additionally display log output in stdout.
Enabled by default.

`log_to_stdout yes`

#### log\_syslog yes/no

Log to system logger.

To enable syslog(3) usage, set log\_syslog to 'yes'. Additionally set
log\_syslog\_ident and log\_syslog\_facility.

`log_syslog no`

#### log\_syslog\_ident "string"

Set syslog ident name.

`log_syslog_ident "odyssey"`

#### log\_syslog\_facility "string"

Set syslog facility name.

`log_syslog_facility "daemon"`

#### log\_debug yes/no

Enable verbose logging of all events, which will generate a log of
detailed information useful for development or testing.

It is also possible to enable verbose logging for specific users
(see routes section).

`log_debug no`

#### log\_config yes/no

Write configuration to the log during start and config reload.

`log_config yes`

#### log\_session yes/no

Write client connect and disconnect events to the log.

`log_session yes`

#### log\_query yes/no

Write client queries text to the log. Disabled by default.

`log_query no`

#### log\_stats yes/no

Periodically display information about active routes.

`log_stats yes`

#### stats\_interval integer

Set interval in seconds for internal statistics update and log report.

`stats_interval 3`

### Performance

#### workers integer

Set size of thread pool used for client processing.

1: By default, Odyssey runs with a single worker. This is a special
mode optimized for general use. This mode also made to reduce multi-thread
communication overhead.

N: Add additional worker threads, if your server experience heavy load,
especially using TLS setup.

`workers 1`

#### resolvers integer

Number of threads used for DNS resolving. This value can be increased, if
your server experience a big number of connecting clients.

`resolvers 1`

#### readahead integer

Set size of per-connection buffer used for io readahead operations.

`readahead 8192`

#### pipeline integer

Set size of buffer used for pipelining io operations between client
and server. Odyssey will try to read as much as 'pipeline' data before sending
data to a peer.

This option differs from 'readahead' since it works with full PostgreSQL
packets. Incoming packet can be larger than pipeline size, in that case buffer
will be enlarged.

It is a good idea to set this value to a approximate max size of
data packet to reduce performance influence of a system memory allocator
(like fragmentation, increased memory usage, etc).

`pipeline 32768`

#### cache integer

Set size of pipeline cache pool (numbers). Approximate cache size could be
calculated as 'cache' * 'pipeline'.

Set to zero, to disable pipeline caching at all.

`cache 100`

#### cache\_chunk integer

Pipeline buffer free watermark value.

If pipeline buffer becomes bigger than `cache_chunk` free it, instead of
putting back to cache.

Set to zero, to disable the check.

`cache_chunk 0`

#### cache\_coroutine integer

Set pool size of free coroutines cache. It is a good idea to set
this value to a sum of max clients plus server connections. Please note, that
each coroutine consumes around 16KB of memory.

Set to zero, to disable coroutine cache.

`cache_coroutine 128`

#### nodelay yes/no

TCP nodelay. Set to 'yes', to enable nodelay.

`nodelay yes`

#### keepalive 7200

TCP keepalive time. Set to zero, to disable keepalive.

`keepalive 7200`

###
### GLOBAL LIMITS
###

#
# Global limit of client connections.
#
# Comment 'client_max' to disable the limit. On client limit reach, Odyssey will
# reply with 'too many connections'.
#
# client_max 100

###
### LISTEN
###

#
# Listen section defines listening servers used for accepting
# incoming client connections.
#
# It is possible to define several Listen sections. Odyssey will listen on
# every specified address.
#
# Odyssey will fail in case it could not bind on any resolved address.
#

listen {
#
# 	Bind address.
	host "*"
#
#	Listen port.
	port 6432
#
#	TCP listen backlog.
	backlog 128
#
#	TLS support.
#
#	Supported TLS modes:
#
#	"disable"     - disable TLS protocol
#	"allow"       - switch to TLS protocol on request
#	"require"     - TLS clients only
#	"verify_ca"   - require valid client certificate
#	"verify_full" - require valid client ceritifcate
#
#	tls "disable"
#	tls_cert_file ""
#	tls_key_file ""
#	tls_ca_file ""
#	tls_protocols ""
}

###
### ROUTING
###

#
# Odyssey allows to define client routing rules by specifying
# 'database', 'user' and 'storage' sections.
#
# On client accept appropriate route is assigned by matching 'database' and
# 'user' sections, all requests then forwarded to a 'storage'
# (which is referenced from the 'user' section).
#
# Database <name> | default.
#
# Defines database name requested by client. Each 'database' section structure
# consist of a 'user' subsections.
#
# A special 'database default' is used, in case when no database is matched.
#
# User <name> | default.
#
# Defines authentication, pooling and storage settings for
# requested route.
#
# A special 'user default' is used, in case when no user is matched.
#
# Storage <name>.
#
# Defines server used as a data storage or admin console operations.
#

storage "postgres_server" {
#
#	Storage type.
#
#	"remote" - PostgreSQL server
#	"local"  - Odyssey (admin console)
#
	type "remote"
#
#	Remote server address.
	host "127.0.0.1"
#
#	Remote server port.
	port 5432
#
#	Remote server TLS settings.
#
#	tls "disable"
#	tls_ca_file ""
#	tls_key_file ""
#	tls_cert_file ""
#	tls_protocols ""
}

database default {
	user default {
#
#		Authentication method.
#
#		"none"       - authentication turned off
#		"block"      - block this user
#		"clear_text" - PostgreSQL clear text authentication
#		"md5"        - PostgreSQL MD5 authentication
#
		authentication "none"

#
#		Authentication method password.
#
#		Depending on selected method, password can be in plain text or md5 hash.
#
#		password ""

#
#		Authentication query.
#
#		Use selected 'auth_query_db' and 'auth_query_user' to match a route.
#		Use matched route server to send 'auth_query' to get username and password needed
#		to authenticate a client.
#
#		auth_query "select username, pass from auth where username='%u'"
#		auth_query_db ""
#		auth_query_user ""

#
#		Client connections limit.
#
#		Comment 'client_max' to disable the limit. On client limit reach, Odyssey will
#		reply with 'too many connections'.
#
#		client_max 100

#
#		Remote server to use.
#
#		By default route database and user names are used as connection
#		parameters to remote server. It is possible to override this values
#		by specifying 'storage_db' and 'storage_user'. Remote server password
#		can be set using 'storage_password' field.
#
		storage "postgres_server"
#		storage_db "database"
#		storage_user "test"
#		storage_password "test"

#
#		Server pool mode.
#
#		"session"     - assign server connection to a client until it disconnects
#		"transaction" - assign server connection to a client for a transaction processing
#
		pool "transaction"

#
#		Server pool size.
#
#		Keep the number of servers in the pool as much as 'pool_size'.
#		Clients are put in a wait queue, when all servers are busy.
#
#		Set to zero to disable the limit.
#
		pool_size 0

#
#		Server pool wait timeout.
#
#		Time to wait in milliseconds for an available server.
#		Disconnect client on timeout reach.
#
#		Set to zero to disable.
#
		pool_timeout 0

#
#		Server pool idle timeout.
#
#		Close an server connection when it becomes idle for 'pool_ttl' seconds.
#
#		Set to zero to disable.
#
		pool_ttl 60

#
#		Server pool auto-cancel.
#
#		Start additional Cancel connection in case if server left with
#		executing query. Close connection otherwise.
#
		pool_cancel yes

#
#		Server pool auto-rollback.
#
#		Execute 'ROLLBACK' if server left in active transaction.
#		Close connection otherwise.
#
		pool_rollback yes

#
#		Forward PostgreSQL errors during remote server connection.
#
		client_fwd_error no

#
#		Enable verbose mode for a specific route only.
#
		log_debug no
	}
}

###
### ADMIN CONSOLE (example)
###

#storage "local" {
#	type "local"
#}

#database "console" {
#	user default {
#		authentication "none"
#		pool "session"
#		storage "local"
#	}
T#}
