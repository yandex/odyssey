# Rules section

Odyssey allows to define client routing rules by specifying
`database`, `user` and `storage` sections.

On client accept appropriate route is assigned by matching `database` and
`user` sections, all requests then forwarded to a `storage`
(which is referenced from the `user` section).

`database <name> | default { users }`

Defines database name requested by client. Each `database` section structure
consist of a `user` subsections.

A special `database default` is used, in case when no database is matched.

`user <name> | default { options }`

Defines authentication, pooling and storage settings for
requested route.

A special `user default` is used, in case when no user is matched.

---

## **authentication**
*string*

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

## **password**
*string*

Set route authentication password. Depending on selected method, password can be
in plain text, md5 hash or SCRAM secret.

To generate SCRAM secret you can use [this](https://github.com/DenisMedeirosBBD/PostgresSCRAM256PasswordGenerator) tool.

`password "test"`

## **auth\_common\_name**
default|*string*

Specify common names to check for "cert" authentication method.
If there are more then one common name is defined, all of them
will be checked until match.

Set 'default' to check for current user.

```
auth_common_name default
auth_common_name "test"
```

## **auth\_query**
*string*

Enable remote route authentication. Use some other route to authenticate clients
following this logic:

Use selected 'auth\_query\_db' and 'auth\_query\_user' to match a route.
Use matched route server to send 'auth\_query' to get username and password needed
to authenticate a client.

```
auth_query "SELECT usename, passwd FROM pg_shadow WHERE usename=$1"
auth_query_db ""
auth_query_user ""
```

Disabled by default.


## **auth\_pam\_service**

Enables PAM(Pluggable Authentication Modules) as the authentication mechanism.
It is incompatible to use it with auth query method. Password must be passed in plain text form, as
standard postgreSQL requires to.

```
auth_pam_service "name desired pam service"
```

## **client\_max**
*integer*

Set client connections limit for this route.

Comment 'client\_max' to disable the limit. On client limit reach, Odyssey will
reply with 'too many connections'.

`client_max 100`

## **storage**
*string*

Set remote server to use.

By default route database and user names are used as connection
parameters to remote server. It is possible to override this values
by specifying `storage_db` and `storage_user`. Remote server password
can be set using `storage_password` field.

```plain
storage "postgres_server"
storage_db "database"
storage_user "test"
storage_password "test"
```
## **ldap_storage_credentials**

This subsection must located at subsection `user` and used to route clients to a remote PostgreSQL server with special credentials 
(`storage_user` and `storage_password`), depending in the client account attributes stored on the LDAP server 
(based on OpenLDAP, Active Directory or others). This routing method allows to grant access 
with different privileges to different databases located on the same host. 

This routing method maybe used only if variables of ldap_endpoint_name and ldap_storage_credentials_attr 
are set. For example:

```
storage "test_server" {
     type "remote"
     port 5432
     host "postgres_server"
}
ldap_endpoint "ldap1" {
     ldapscheme "ldap"
     ldapbasedn "dc=example,dc=org"
     ldapbinddn "cn=admin,dc=example,dc=org"
     ldapbindpasswd "admin"
     ldapsearchfilter "(memberOf=cn=localhost,ou=groups,dc=example,dc=org)"
     ldapsearchattribute "gecos"
     ldapserver "192.168.233.16"
     ldapport 389
}
database default {
    user default {
          authentication "clear_text"
          storage "test_server"
	    
          ldap_endpoint_name "ldap1"
          ldap_storage_credentials_attr "memberof"
          ldap_storage_credentials "group_ro" {
               ldap_storage_username "ldap_ro"
               ldap_storage_password "password1"
          }
          ldap_storage_credentials "group_rw" {
               ldap_storage_username "ldap_rw"
               ldap_storage_password "password2
          }
	    
          #other required regular parameters are hidden from this example
     }
}
```
To successfully route client to PostgreSQL server with correct credentials, client account attributes
stored on LDAP server must contain three required values separated by `_` character:
hostname of PostgreSQL server (`host` value from `storage` section), name of target `database`,
and name of `ldap_storage_credentials` in format `%host_%database_%ldap_storage_credentials`
For example, look at `memberof` attributes in [usr4.ldiff](https://github.com/yandex/odyssey/tree/master/docker/ldap):
```
dn: uid=user4,dc=example,dc=org
objectClass: top
objectClass: account
objectClass: posixAccount
objectClass: shadowAccount
cn: user4
uid: user4
memberof: cn=localhost,ou=groups,dc=example,dc=org
memberof: cn=localhost_ldap_db1_group_ro,ou=groups,dc=example,dc=org
memberof: cn=localhost_ldap_db2_group_rw,ou=groups,dc=example,dc=org
uidNumber: 16860
gidNumber: 101
homeDirectory: /home/user4
loginShell: /bin/bash
gecos: user4
userPassword: default
shadowLastChange: 0
shadowMax: 0
shadowWarning: 0
```

## **ldap_storage_credentials_attr**
*string*

Sets the value of the account attribute name from the LDAP server, the values 
of which will be used to determine the route and parameters for connecting the client to the PostgreSQL server.

## **ldap_endpoint_name**
*string*

Specifies the name of ldap_endpoint to be used to connect to the LDAP server.

## **ldap_endpoint**

The ldap_endpoint section is used to configure the parameters for connecting to the LDAP server. For example:

```
ldap_endpoint "ldap1" {
	ldapscheme "ldap"
	ldapbasedn "dc=example,dc=org"
	ldapbinddn "cn=admin,dc=example,dc=org"
	ldapbindpasswd "admin"
	ldapsearchattribute "gecos"
	ldapserver "192.168.233.16"
	ldapport 389
}
```

## **ldap\_pool\_size**
*integer*

Ldap server pool size

Keep the number of servers in the pool as much as 'pool\_size'.
Clients are put in a wait queue, when all servers are busy.

Set to zero to disable.

`ldap_pool_size 10`

## **ldap\_pool\_timeout**
*integer*

Ldap server pool timeout

Time to wait in milliseconds for an available server.
Disconnect client on timeout reach.

Set to zero to disable.

`ldap_pool_timeout 1000`

## **ldap\_pool\_ttl**
*integer*

Ldap server pool idle timeout.

Close ldap server connection when it becomes idle for 'ldap\_pool\_ttl' seconds.

Set to zero to disable.

`ldap_pool_ttl 60`

## **password_passthrough**
*bool*

By default odyssey authenticate users itself, but if side auth application is used,
like LDAP server, PAM module, or custom auth module, sometimes, 
instead of configuring `storage_password`, it is more convenient to reuse
client-provided password to perform backend auth. If you set this option to "yes"
Odyssey will store client token and use when new server connection is Opened. Anyway, if
you configure `storage_password` for route, `password_passthrough` is essentially ignored


## **pool**
*string*

Set route server pool mode.

Supported modes:

```
"session"     - assign server connection to a client until it disconnects
"transaction" - assign server connection to a client for a transaction processing
```

`pool "transaction"`

## **pool\_size**
*integer*

Server pool size.

Keep the number of servers in the pool as much as 'pool\_size'.
Clients are put in a wait queue, when all servers are busy.

Set to zero to disable the limit.

`pool_size 100`

## **min_pool_size**
*integer*

Minumum server pool size.

Keep the number of servers in the pool at minimum as 'min_pool_size'.
Helps to handle unexpected high load.
Default: 0

`min_pool_size 50`

## **pool\_timeout**
*integer*

Server pool wait timeout.

Time to wait in milliseconds for an available server.
Disconnect client on timeout reach.

Set to zero to disable.

`pool_timeout 4000`

## **pool\_ttl**
*integer*

Server pool idle timeout.

Close an server connection when it becomes idle for 'pool\_ttl' seconds.

Set to zero to disable.

`pool_ttl 60`

## **pool\_discard**
*yes|no*

Server pool parameters discard.

Execute `DISCARD ALL` and reset client parameters before using server
from the pool.

`pool_discard no`

## **pool\_smart\_discard**
*yes|no*

When this parameter is enabled, Odyssey sends smart discard query instead of default `DISCARD ALL` when it
returns connection to the pool. Its default value may be overwritten by pool\_discard\_string setting.

## **pool\_discard\_string**
*string*

When resetting a database connection, a pre-defined query string is sent to the server. This query string consists of a set of SQL statements that will be executed during a `DISCARD ALL` command, except for `DEALLOCATE ALL`. The default query string includes the following statements:

```sql
SET SESSION AUTHORIZATION DEFAULT;
RESET ALL;
CLOSE ALL;
UNLISTEN *;
SELECT pg_advisory_unlock_all();
DISCARD PLANS;
DISCARD SEQUENCES;DISCARD TEMP;
```

This sequence of statements is designed to reset the connection to a clean state, without affecting the authentication credentials of the session. By executing these queries, any open transactions will be closed, locks will be released, and any cached execution plans and sequences will be discarded.

## pool\_ignore\_discardall *yes|no*

Server-pool parameter: ignore client DISCARD.

Skips client-side `DISCARD ALL` statements instead of forwarding them to the
backend (preventing the command from reaching PostgreSQL).

Odyssey intercepts every simple query whose text is exactly `DISCARD ALL`
(case-insensitive, with optional whitespace or a trailing semicolon) and
immediately returns `CommandComplete: DISCARD` + `ReadyForQuery` to the client.
The query never reaches PostgreSQL, so no session reset or plan-cache flush
occurs on the server side.

This is useful when the client driver in use offers no option to disable its
automatic `DISCARD ALL` calls.

`pool_ignore_discardall no`

## **pool\_cancel**
*yes|no*

Server pool auto-cancel.

Start additional Cancel connection in case if server left with
executing query. Close connection otherwise.

`pool_cancel no`

## **pool\_rollback**
*yes|no*

Server pool auto-rollback.

Execute 'ROLLBACK' if server left in active transaction.
Close connection otherwise.

`pool_rollback yes`

## **client\_fwd\_error**
*yes|no*

Forward PostgreSQL errors during remote server connection.

`client_fwd_error no`

## **application_name_add_host**
*yes|no*

Add client host name to application_name parameter

`application_name_add_host yes`

## **reserve_session_server_connection**
*yes|no*

Connect new client to server immediately or wait for first query

`reserve_session_server_connection yes`

## **server_lifetime**
*integer*

Server lifetime - maximum number of seconds for a server connection to live. Prevents cache bloat.
Server connection is deallocated only in idle state.
Defaults to 3600 (1 hour)
Use 0 to disable

`server_lifetime 3600`

## **pool\_client\_idle\_timeout**
*integer*

Client pool idle timeout.

Drop stale client connection after this much seconds of idleness, which is not in transaction.

Set to zero to disable.

`pool_client_idle_timeout 0`

## **pool\_idle\_in\_transaction\_timeout**
*integer*

Client pool idle in transaction timeout.

Drop client connection in transaction after this much seconds of idleness.

Set to zero to disable.

`pool_idle_in_transaction_timeout 0`

## **pool_reserve_prepared_statement**
*yes/no**

Enable support of prepared statements in transactional pooling.

`pool_reserve_prepared_statement yes`

## **log\_debug**
*yes|no*

Enable verbose mode for a specific route only.

`log_debug no`

## **group\_checker\_interval**
*integer*

Soft interval between group checks (in ms)
7000 by default

`group_checker_interval 7000`

## **maintain_params**
*yes|no*

User parameters maintenance

By default, odyssey saves parameters values defined by user
and deploys them on server attach, if they are different from servers.
This options disable feature.

`maintain_params no`

## **target_session_attrs**
*string*

Target session attrs feature. Odyssey will lookup for primary/standby
for connections of this user, depending on value set.
Possible values are:

- read-write - always select host, available for write
- read-only - never select host, available for write
- any (the default one) - select host randomly

`target_session_attrs "read-write"`

## **quantiles**
*string*

Compute quantiles of query and transaction times

`quantiles "0.99,0.95,0.5"`


## **catchup_timeout**
*integer*

Specify maximum replication lag in second for the user
See [catchup-timeout.md](../features/catchup-timeout.md) for more details

`catchup_timeout 10`

## **catchup_checks**
*integer*

Maximum amount of cachtup checks before closing
the connection if host replication lag is too big
See [catchup-timeout.md](../features/catchup-timeout.md) for more details

`catchup_checks 10`

## example (remote)

```
database default {
	user default {
		authentication "none"
#		password ""
#		auth_common_name default
#		auth_common_name "test"
#		auth_query "SELECT usename, passwd FROM pg_shadow WHERE usename=$1"
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

## example (admin console)

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
