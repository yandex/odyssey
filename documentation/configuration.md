
### Odyssey configuration file reference

#### include *string*

Include one or more configuration files. Include files can
include other files.

`include "path"`

#### daemonize *yes|no*

Start as a daemon.

By default Odyssey does not run as a daemon. Set to 'yes' to enable.

`daemonize no`

#### priority *integer*

Process priority.

Set Odyssey parent process and threads priority.

`priority -10`

#### pid\_file *string*

If pid\_file is specified, Odyssey will write its process id to
the specified file at startup.

`pid_file "/var/run/odyssey.pid"`

#### unix\_socket\_dir *string*

UNIX socket directory.

If `unix_socket_dir` is specified, Odyssey will enable UNIX socket
communications. Specified directory path will be used for
searching socket files.

`unix_socket_dir "/tmp"`

#### unix\_socket\_mode *string*

Set `unix_socket_mode` file mode to any created unix files.

`unix_socket_mode "0755"`

#### log\_file *string*

If log\_file is specified, Odyssey will additionally use it to write
log events.

`log_file "/var/log/odyssey.log"`

#### log\_format *string*

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

#### log\_to\_stdout *yes|no*

Set to 'yes' if you need to additionally display log output in stdout.
Enabled by default.

`log_to_stdout yes`

#### log\_syslog *yes|no*

Log to system logger.

To enable syslog(3) usage, set log\_syslog to 'yes'. Additionally set
log\_syslog\_ident and log\_syslog\_facility.

`log_syslog no`

#### log\_syslog\_ident *string*

Set syslog ident name.

`log_syslog_ident "odyssey"`

#### log\_syslog\_facility *string*

Set syslog facility name.

`log_syslog_facility "daemon"`

#### log\_debug *yes|no*

Enable verbose logging of all events, which will generate a log of
detailed information useful for development or testing.

It is also possible to enable verbose logging for specific users
(see routes section).

`log_debug no`

#### log\_config *yes|no*

Write configuration to the log during start and config reload.

`log_config yes`

#### log\_session *yes|no*

Write client connect and disconnect events to the log.

`log_session yes`

#### log\_query *yes|no*

Write client queries text to the log. Disabled by default.

`log_query no`

#### log\_stats *yes|no*

Periodically display information about active routes.

`log_stats yes`

#### stats\_interval *integer*

Set interval in seconds for internal statistics update and log report.

`stats_interval 3`

#### workers *integer*

Set size of thread pool used for client processing.

1: By default, Odyssey runs with a single worker. This is a special
mode optimized for general use. This mode also made to reduce multi-thread
communication overhead.

N: Add additional worker threads, if your server experience heavy load,
especially using TLS setup.

`workers 1`

#### resolvers *integer*

Number of threads used for DNS resolving. This value can be increased, if
your server experience a big number of connecting clients.

`resolvers 1`

#### readahead *integer*

Set size of per-connection buffer used for io readahead operations.

`readahead 8192`

#### cache\_coroutine *integer*

Set pool size of free coroutines cache. It is a good idea to set
this value to a sum of max clients plus server connections. Please note, that
each coroutine consumes around 16KB of memory.

Set to zero, to disable coroutine cache.

`cache_coroutine 128`

#### nodelay *yes|no*

TCP nodelay. Set to 'yes', to enable nodelay.

`nodelay yes`

#### keepalive *integer*

TCP keepalive time. Set to zero, to disable keepalive.

`keepalive 7200`

#### coroutine\_stack\_size *integer*

Coroutine stack size.

Set coroutine stack size in pages. In some rare cases
it might be necessary to make stack size bigger. Actual stack will be
allocated as `(coroutine_stack_size + 1_guard_page) * page_size`.
Guard page is used to track stack overflows. Stack by default is set to 16KB.

`coroutine_stack_size 4`

#### client\_max *integer*

Global limit of client connections.

Comment 'client_max' to disable the limit. On client limit reach, Odyssey will
reply with 'too many connections'.

`client_max 100`

### Listen

Listen section defines listening servers used for accepting
incoming client connections.

It is possible to define several Listen sections. Odyssey will listen on
every specified address.

Odyssey will fail in case it could not bind on any resolved address.

#### host *string*

If host is not set, Odyssey will try to listen using UNIX socket if
`unix_socket_dir` is set.

`host "*"`

#### port *integer*

`port 6432`

#### backlog *integer*

`backlog 128`

#### tls *string*

Supported TLS modes:

```
"disable"     - disable TLS protocol
"allow"       - switch to TLS protocol on request
"require"     - TLS clients only
"verify_ca"   - require valid client certificate
"verify_full" - require valid client ceritifcate
```

#### example

```
listen
{
	host "*"
	port 6432
	backlog 128
#	tls "disable"
#	tls_cert_file ""
#	tls_key_file ""
#	tls_ca_file ""
#	tls_protocols ""
}
```

### Routing rules

Odyssey allows to define client routing rules by specifying
`database`, `user` and `storage` sections.

On client accept appropriate route is assigned by matching `database` and
`user` sections, all requests then forwarded to a `storage`
(which is referenced from the `user` section).

### Storage

Defines server used as a data storage or admin console operations.

`storage <name> { options }`

#### type *string*

Set storage type to use. Supported types:

```
"remote" - PostgreSQL server
"local"  - Odyssey (admin console)
```

`type "remote"`

#### host *string*

Remote server address.

If host is not set, Odyssey will try to connect using UNIX socket if
`unix_socket_dir` is set.

#### port *integer*

Remote server port.

#### tls *string*

Supported TLS modes:

```
"disable"     - disable TLS protocol
"allow"       - switch to TLS protocol on request
"require"     - TLS required
"verify_ca"   - require valid certificate
"verify_full" - require valid ceritifcate
```

#### example

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

### Database and User

`database <name> | default { users }`

Defines database name requested by client. Each `database` section structure
consist of a `user` subsections.

A special `database default` is used, in case when no database is matched.

`user <name> | default { options }`

Defines authentication, pooling and storage settings for
requested route.

A special `user default` is used, in case when no user is matched.

#### authentication *string*

Set route authentication method. Supported:

```
"none"       	- authentication turned off
"block"      	- block this user
"clear_text" 	- PostgreSQL clear text authentication
"md5"        	- PostgreSQL md5 authentication
"scram-sha-256" - PostgreSQL scram-sha-256 authentication
"cert"       	- Compare client certificate Common Name against auth_common_name's
```

`authentication "none"`

#### password *string*

Set route authentication password. Depending on selected method, password can be
in plain text, md5 hash or SCRAM secret.

To generate SCRAM secret you can use [this](https://github.com/DenisMedeirosBBD/PostgresSCRAM256PasswordGenerator) tool.

`password "test"`

#### auth\_common\_name default|*string*

Specify common names to check for "cert" authentication method.
If there are more then one common name is defined, all of them
will be checked until match.

Set 'default' to check for current user.

```
auth_common_name default
auth_common_name "test"
```

#### auth\_query *string*

Enable remote route authentication. Use some other route to authenticate clients
following this logic:

Use selected 'auth\_query_db' and 'auth\_query\_user' to match a route.
Use matched route server to send 'auth\_query' to get username and password needed
to authenticate a client.

```
auth_query "select username, pass from auth where username='%u'"
auth_query_db ""
auth_query_user ""
```

Disabled by default.


#### auth\_pam\_service

Enables PAM(Pluggable Authentication Modules) as the authentication mechanism.
It is incompatible to use it with auth query method. Password must be passed in plain text form, as
standard postgreSQL requires to.

```
auth_pam_service "name desired pam service"
```

#### client\_max *integer*

Set client connections limit for this route.

Comment 'client\_max' to disable the limit. On client limit reach, Odyssey will
reply with 'too many connections'.

`client_max 100`

#### storage *string*

Set remote server to use.

By default route database and user names are used as connection
parameters to remote server. It is possible to override this values
by specifying 'storage_db' and 'storage_user'. Remote server password
can be set using 'storage_password' field.

```
storage "postgres_server"
#storage_db "database"
#storage_user "test"
#storage_password "test"
```

#### pool *string*

Set route server pool mode.

Supported modes:

```
"session"     - assign server connection to a client until it disconnects
"transaction" - assign server connection to a client for a transaction processing
```

`pool "transaction"`

#### pool\_size *integer*

Server pool size.

Keep the number of servers in the pool as much as 'pool\_size'.
Clients are put in a wait queue, when all servers are busy.

Set to zero to disable the limit.

`pool_size 100`

#### pool\_timeout *integer*

Server pool wait timeout.

Time to wait in milliseconds for an available server.
Disconnect client on timeout reach.

Set to zero to disable.

`pool_timeout 4000`

#### pool\_ttl *integer*

Server pool idle timeout.

Close an server connection when it becomes idle for 'pool\_ttl' seconds.

Set to zero to disable.

`pool\_ttl 60`

#### pool\_discard *yes|no*

Server pool parameters discard.

Execute `DISCARD ALL` and reset client parameters before using server
from the pool.

`pool_discard no`

#### pool\_cancel *yes|no*

Server pool auto-cancel.

Start additional Cancel connection in case if server left with
executing query. Close connection otherwise.

`pool_cancel no`

#### pool\_rollback *yes|no*

Server pool auto-rollback.

Execute 'ROLLBACK' if server left in active transaction.
Close connection otherwise.

`pool_rollback yes`

#### client\_fwd\_error *yes|no*

Forward PostgreSQL errors during remote server connection.

`client_fwd_error no`

#### log\_debug *yes|no*

Enable verbose mode for a specific route only.

`log_debug no`

#### example (remote)

```
database default {
	user default {
		authentication "none"
#		password ""
#		auth_common_name default
#		auth_common_name "test"
#		auth_query "select username, pass from auth where username='%u'"
#		auth_query_db ""
#		auth_query_user ""
#		client_max 100

		storage "postgres_server"
#		storage_db "database"
#		storage_user "test"
#		storage_password "test"

		pool "transaction"
		pool_size 0
		pool_timeout 0
		pool_ttl 60
		pool_cancel no
		pool_rollback yes

		client_fwd_error no
		log_debug no
	}
}
```

#### example (admin console)

```
storage "local" {
	type "local"
}

database "console" {
	user default {
		authentication "none"
		pool "session"
		storage "local"
	}
}
```
