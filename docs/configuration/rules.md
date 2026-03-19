# Rules section

Odyssey allows you to define client routing rules by specifying
`database`, `user`, and `storage` sections.

When a client connects, the appropriate route is assigned by matching `database` and
`user` sections. All requests are then forwarded to a `storage`
(which is referenced from the `user` section).

`database <name> | default { users }`

Defines database name requested by client. Each `database` section structure
consists of `user` subsections.

A special `database default` is used when no database is matched.

`user <name> | default { options }`

Defines authentication, pooling and storage settings for
requested route.

A special `user default` is used when no user is matched.

---


## Configuration Parameters Reference
| Parameter                         | Type                                   | Default       | Reload                    | Notes                                                                                                                                                                      |
|-----------------------------------|----------------------------------------|---------------|---------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| authentication                    | string (none/block/clear_text/external/md5/scram-sha-256/cert) | — (not set)   | runtime (new connections) | Must be set per rule; validated to one of: none, block, clear_text, md5, scram-sha-256, cert. If omitted, config validation errors ("authentication mode is not defined"). |
| password                          | string                                 | — (not set)   | runtime (new connections) | Route auth secret (plain, MD5 hash, or SCRAM secret).                                                                                                                      |
| auth_common_name                  | string/list + keyword default          | — (none)      | runtime (new connections) | For cert auth. You can list CNs; special keyword default toggles "accept any CN" for certificates.                                                                         |
| auth_query                        | string                                 | — (not set)   | runtime (new connections) | External auth SQL query; optional parameter for custom authentication logic.                                                                                               |
| auth_pam_service                  | string                                 | — (not set)   | runtime (new connections) | PAM service name (only available if PAM support is compiled in).                                                                                                           |
| client_max                        | integer                                | 0             | runtime (new connections) | Per-route client connection limit; 0 = unlimited connections.                                                                                                              |
| storage                           | string                                 | — (not set)   | runtime (new connections) | Storage block reference containing backend connection details (endpoints, TLS, etc.). Must be configured for the route to be usable.                                       |
| storage_db                        | string                                 | — (not set)   | runtime (new connections) | Overrides database name for backend connections.                                                                                                                           |
| storage_user                      | string                                 | — (not set)   | runtime (new connections) | Overrides username for backend connections.                                                                                                                                |
| storage_password                  | string                                 | — (not set)   | runtime (new connections) | Password for backend connections.                                                                                                                                          |
| ldap_endpoint_name                | string                                 | — (not set)   | runtime (new connections) | LDAP endpoint name used when LDAP authentication is enabled.                                                                                                               |
| ldap_storage_credentials_attr     | string                                 | — (not set)   | runtime (new connections) | LDAP attribute name to extract storage credentials from LDAP response.                                                                                                     |
| ldap_pool_size                    | integer                                | 0             | runtime (new connections) | LDAP connection pool size; 0 = no pooling.                                                                                                                                 |
| ldap_pool_timeout                 | integer (ms)                           | 0             | runtime (new connections) | Timeout for getting connection from LDAP pool.                                                                                                                             |
| ldap_pool_ttl                     | integer (sec)                          | 0             | runtime (new connections) | Time-to-live for idle connections in LDAP pool.                                                                                                                            |
| password_passthrough              | boolean (yes/no)                       | no (0)        | runtime (new connections) | Enables password passthrough mode; allows forwarding client credentials to backend.                                                                                        |
| pool                              | string (session/transaction/statement) | — (not set)   | runtime (new connections) | Required: connection pooling mode. Must be explicitly configured.                                                                                                          |
| pool_size                         | integer                                | 0             | runtime (new connections) | Maximum connections in pool; 0 = unlimited.                                                                                                                                |
| min_pool_size                     | integer                                | 0             | runtime (new connections) | Minimum connections to maintain in pool.                                                                                                                                   |
| pool_timeout                      | integer (ms)                           | 0             | runtime (new connections) | Maximum wait time for acquiring connection from pool.                                                                                                                      |
| pool_ttl                          | integer (sec)                          | 0             | runtime (new connections) | Time-to-live for idle server connections.                                                                                                                                  |
| pool_discard                      | boolean                                | yes (1)       | runtime (new connections) | Execute DISCARD ALL when returning connections to pool.                                                                                                                    |
| pool_smart_discard                | boolean                                | no (0)        | runtime (new connections) | Use custom discard query instead of DISCARD ALL when enabled.                                                                                                              |
| pool_discard_string               | string                                 | — (not set)   | runtime (new connections) | Custom discard query; if includes DEALLOCATE ALL, prepared statements are disallowed.                                                                                      |
| pool_cancel                       | boolean                                | yes (1)       | runtime (new connections) | Send cancel request to backend when client disconnects.                                                                                                                    |
| pool_rollback                     | boolean                                | yes (1)       | runtime (new connections) | Execute ROLLBACK when returning connections with open transactions.                                                                                                        |
| client_fwd_error                  | boolean                                | no (0)        | runtime (new connections) | Forward backend connection errors to client during connection establishment.                                                                                               |
| sql_guard { ... }                 | block                                  | — (disabled)  | runtime (new connections) | SQL guard block: `mode` ("blacklist"/"whitelist"), `cache` (yes/no), `monitoring` (yes/no), `regex` (POSIX ERE, multiple allowed, combined with OR). |
| application_name_add_host         | boolean                                | no (0)        | runtime (new connections) | Append client hostname to application_name parameter.                                                                                                                      |
| reserve_session_server_connection | boolean                                | yes (1)       | runtime (new connections) | Immediately establish backend connection when client connects.                                                                                                             |
| server_lifetime                   | integer (sec)                          | 3600          | runtime (new connections) | Maximum lifetime for backend connections (1 hour default).                                                                                                                 |
| pool_client_idle_timeout          | integer (ms/us)                        | 0             | runtime (new connections) | Timeout for idle client connections; only applies to session pooling mode.                                                                                                 |
| pool_idle_in_transaction_timeout  | integer                                | 0             | runtime (new connections) | Timeout for idle clients with open transactions; session pooling only.                                                                                                     |
| pool_reserve_prepared_statement   | boolean                                | yes (0)       | runtime (new connections) | Enable prepared statement support; incompatible with session pooling and certain discard modes.                                                                            |
| pool_pin_on_listen          | boolean                        | no (0)             | runtime (new connections) | Enable pinning client to server after LISTEN execution                                                                                    |
| log_debug                         | boolean                                | no (0)        | runtime (new connections) | Enable debug logging for this route.                                                                                                                                       |
| group_checker_interval            | integer (ms)                           | 7000 (global) | runtime (global)          | Global setting: interval for checking group membership changes (7 seconds default).                                                                                        |
| maintain_params                   | boolean                                | yes (1)       | runtime (new connections) | Maintain client connection parameters across backend connections for compatibility.                                                                                        |
| target_session_attrs              | string enum                            | — (not set)   | runtime (new connections) | Target session attributes for connection routing; defaults to undefined behavior.                                                                                          |
| quantiles                         | string (comma-separated)               | — (not set)   | runtime (new connections) | Comma-separated list of quantile values for statistics collection; disabled when not set.                                                                                  |
| catchup_timeout                   | integer (sec)                          | 0             | runtime (new connections) | Timeout for replica catchup operations; 0 = no timeout.                                                                                                                    |
| catchup_checks                    | integer                                | 0             | runtime (new connections) | Maximum number of catchup checks; 0 = no limit.                                                                                                                            |

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

---

## **password**

*string*

Set route authentication password. Depending on selected method, password can be
in plain text, md5 hash or SCRAM secret.

To generate SCRAM secret you can use [this](https://github.com/DenisMedeirosBBD/PostgresSCRAM256PasswordGenerator) tool.

`password "test"`

---

## **auth\_common\_name**

default|*string*

Specify common names to check for "cert" authentication method.
If more than one common name is defined, all of them
will be checked until a match is found.

Set 'default' to check for the current user.

```
auth_common_name default
auth_common_name "test"
```

---

## **auth\_query**

*string*

Enable remote route authentication. Use another route to authenticate clients
following this logic:

Use the selected 'auth\_query\_db' and 'auth\_query\_user' to match a route.
Use the matched route server to send 'auth\_query' to get the username and password needed
to authenticate a client.

```
auth_query "SELECT usename, passwd FROM pg_shadow WHERE usename=$1"
auth_query_db ""
auth_query_user ""
```

Disabled by default.

---

## **auth\_pam\_service**

Enables PAM (Pluggable Authentication Modules) as the authentication mechanism.
It is incompatible with the auth query method. The password must be passed in plain text form, as
standard PostgreSQL requires.

```
auth_pam_service "name desired pam service"
```

---

## **client\_max**

*integer*

Set client connections limit for this route.

Comment out 'client\_max' to disable the limit. When the client limit is reached, Odyssey will
reply with 'too many connections'.

`client_max 100`

---

## **storage**

*string*

Set remote server to use.

By default, route database and user names are used as connection
parameters to the remote server. It is possible to override these values
by specifying `storage_db` and `storage_user`. The remote server password
can be set using the `storage_password` field.

```plain
storage "postgres_server"
storage_db "database"
storage_user "test"
storage_password "test"
```

---

## **ldap_storage_credentials**

This subsection must be located within the `user` subsection and is used to route clients to a remote PostgreSQL server with special credentials 
(`storage_user` and `storage_password`), depending on the client account attributes stored on the LDAP server 
(based on OpenLDAP, Active Directory or others). This routing method allows to grant access 
with different privileges to different databases located on the same host. 

This routing method may only be used if the variables ldap_endpoint_name and ldap_storage_credentials_attr 
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
               ldap_storage_password "password2"
          }
	    
          #other required regular parameters are hidden from this example
     }
}
```
To successfully route the client to the PostgreSQL server with correct credentials, client account attributes
stored on the LDAP server must contain three required values separated by the `_` character:
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

---

## **ldap_storage_credentials_attr**

*string*

Sets the account attribute name from the LDAP server, whose values
will be used to determine the route and parameters for connecting the client to the PostgreSQL server.

---

## **ldap_endpoint_name**

*string*

Specifies the name of the ldap_endpoint to be used to connect to the LDAP server.

---

## **ldap_endpoint**

The ldap_endpoint section is used to configure parameters for connecting to the LDAP server. For example:

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

---

## **ldap\_pool\_size**

*integer*

LDAP server pool size

Keep the number of servers in the pool up to 'pool\_size'.
Clients are put in a wait queue when all servers are busy.

Set to zero to disable.

`ldap_pool_size 10`

---

## **ldap\_pool\_timeout**

*integer*

LDAP server pool timeout

Time to wait in milliseconds for an available server.
Disconnect the client when the timeout is reached.

Set to zero to disable.

`ldap_pool_timeout 1000`

---

## **ldap\_pool\_ttl**

*integer*

LDAP server pool idle timeout.

Close the LDAP server connection when it becomes idle for 'ldap\_pool\_ttl' seconds.

Set to zero to disable.

`ldap_pool_ttl 60`

---

## **password_passthrough**

*bool*

By default, Odyssey authenticates users itself, but if a side authentication application is used,
such as a LDAP server, PAM module, or custom auth module, sometimes
instead of configuring `storage_password`, it is more convenient to reuse the
client-provided password to perform backend authentication. If you set this option to "yes",
Odyssey will store the client token and use it when a new server connection is opened. However, if
you configure `storage_password` for the route, `password_passthrough` is essentially ignored.

---

## **pool**

*string*

Set route server pool mode.

Supported modes:

```
"session"     - assign server connection to a client until it disconnects
"transaction" - assign server connection to a client for a transaction processing
```

`pool "transaction"`

---

## **pool\_size**

*integer*

Server pool size.

Keep the number of servers in the pool up to 'pool\_size'.
Clients are put in a wait queue when all servers are busy.

Set to zero to disable the limit.

`pool_size 100`

---

## **min_pool_size**

*integer*

Minimum server pool size.

Keep the number of servers in the pool at a minimum of 'min_pool_size'.
Helps to handle unexpected high load.
Default: 0

`min_pool_size 50`

---

## **pool\_timeout**

*integer*

Server pool wait timeout.

Time to wait in milliseconds for an available server.
Disconnect the client when the timeout is reached.

Set to zero to disable.

`pool_timeout 4000`

---

## **pool\_ttl**

*integer*

Server pool idle timeout.

Close a server connection when it becomes idle for 'pool\_ttl' seconds.

Set to zero to disable.

`pool_ttl 60`

---

## **pool\_discard**

*yes|no*

Server pool parameters discard.

Execute `DISCARD ALL` and reset client parameters before using a server
from the pool.

`pool_discard no`

---

## **pool\_smart\_discard**

*yes|no*

When this parameter is enabled, Odyssey sends a smart discard query instead of the default `DISCARD ALL` when it
returns a connection to the pool. Its default value may be overwritten by the pool\_discard\_string setting.

---

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

---

## **pool\_cancel**

*yes|no*

Server pool auto-cancel.

Start an additional Cancel connection if the server is left with an
executing query. Close the connection otherwise.

`pool_cancel no`

---

## **pool\_rollback**

*yes|no*

Server pool auto-rollback.

Execute 'ROLLBACK' if the server is left in an active transaction.
Close the connection otherwise.

`pool_rollback yes`

---

## **client\_fwd\_error**

*yes|no*

Forward PostgreSQL errors during remote server connection.

`client_fwd_error no`

---

## **sql\_guard**

*"blacklist"|"whitelist"*

SQL guard is a lightweight database firewall built into the connection pooler
layer. Similar in concept to
[Oracle SQL Firewall](https://docs.oracle.com/en/database/oracle/oracle-database/23/sqlfw/oracle-sql-firewall.html)
and [DataSunrise Database Firewall](https://www.datasunrise.com/datasunrise-database-firewall/),
it inspects every SQL statement passing through odyssey and enforces
regex-based filtering rules before the query reaches the backend.

### When to use sql\_guard

By design, the primary defense against SQL injection is parameterized queries
(prepared statements). However, there are real-world scenarios where that is
not enough or not feasible:

- **Legacy applications** — existing codebases that construct SQL via string
  concatenation, where refactoring to prepared statements is impractical or
  too risky for a production system.
- **Third-party and vendor software** — applications where you have no control
  over the generated SQL and cannot enforce coding standards.
- **Untrusted access** — when external contractors, auditors, or analytics
  tools connect directly to the database and you need to restrict the set of
  allowed operations beyond what PostgreSQL roles and grants can express
  (e.g. block `COPY ... PROGRAM`, `lo_export`, `CREATE FUNCTION ... LANGUAGE 'c'`).
- **Defense in depth** — even well-written applications benefit from an
  additional layer that blocks known attack patterns at the network edge,
  before the query is parsed by PostgreSQL.
- **Compliance** — regulatory requirements (PCI DSS, SOC 2) may demand
  runtime SQL monitoring and blocking capabilities at the database access layer.

### Syntax

```
sql_guard {
    mode "blacklist"    # or "whitelist"
    cache yes           # optional, default no
    monitoring yes      # optional, default no — log only, don't block
    regex "pattern1"
    regex "pattern2"
}
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `mode` | string | yes | `"blacklist"` blocks matching queries; `"whitelist"` allows only matching queries |
| `cache` | yes/no | no | Enable direct-mapped murmur hash cache (4096 entries) for regex results |
| `monitoring` | yes/no | no | When enabled, queries that would be blocked are only logged (not blocked). Use this to test rules before enforcing them. |
| `regex` | string | yes (at least one) | POSIX ERE pattern; multiple lines are combined with `\|` (OR) at load time |

### Modes

- **blacklist** — queries matching the regex are **blocked**. Use this for
  SQL injection prevention: define patterns for known dangerous constructs
  (DROP TABLE, UNION SELECT, COPY PROGRAM, etc.) and block them.
- **whitelist** — only queries matching the regex are **allowed** through;
  everything else is blocked. Use this for strict environments where only a
  known set of query shapes should reach the database (e.g. read-only
  reporting access limited to SELECT).

The client receives a PostgreSQL error with SQLSTATE 42000 (syntax error or
access rule violation) when a query is blocked.

Matching is **case-insensitive** (`REG_ICASE`), so patterns like
`\bDROP\s+TABLE\b` will match `drop table`, `Drop Table`, etc.

The check is applied to both simple query protocol (`Query` messages) and
extended query protocol (`Parse` messages).

### How the cache works

The cache is a fixed-size array of 4096 slots, allocated once at config load
time when `cache yes` is set inside the sql\_guard block.

**On every query**, odyssey computes a murmur hash of the query text and maps
it to a cache slot (`slot = hash & 4095`):

1. **Cache hit** — if the slot is valid and the stored hash matches, the cached
   `blocked` result (1 or 0) is returned immediately. No regex is executed.

2. **Cache miss** — the regex is executed via `regexec()`. The mode logic is
   then applied:
   - **blacklist**: regex match -> `blocked=1`, no match -> `blocked=0`
   - **whitelist**: regex match -> `blocked=0`, no match -> `blocked=1`

   The final `blocked` result is stored in the cache slot along with the
   query hash. Future identical queries will hit the cache and skip the regex.

**Key properties:**

- The cache stores the **final blocked/allowed decision**, not the raw regex
  match. This means it works correctly for both blacklist and whitelist modes.
- The cache is **direct-mapped** (one slot per hash bucket) and **never fills
  up**. If a new query maps to an already-occupied slot, it simply overwrites
  the previous entry. The evicted query will have a cache miss on its next
  occurrence and re-run the regex — no correctness issue, just one extra
  `regexec()` call. With 4096 slots and typical workloads (tens to hundreds
  of unique queries), collisions are rare. For workloads with thousands of
  distinct queries, the cache still helps — the most frequently repeated
  queries stay cached while rare ones fall through to regex.
- The cache is **shared between workers** with no locking. Races are benign —
  worst case is a redundant `regexec()` call on the next request, which
  produces the same result.
- The cache is **not invalidated** on config reload. After a reload, new
  connections get a fresh rule with a new cache. Existing connections continue
  using their current cache until they disconnect.

### Overhead

Enabling sql\_guard adds ~0.25 us overhead per query for the POSIX regex
check. With `cache yes`, the overhead is reduced to ~0.07 us per query on
repeated workloads (~3x speedup). Both are negligible for a connection pooler
(a typical network round-trip is 100-1000 us). Run `make benchmark-sql-guard`
to measure on your hardware.

### Blacklist example

The regex should be designed to cover known PostgreSQL SQL injection attack
categories. Below is a reference of common attack vectors
(see [PayloadsAllTheThings/PostgreSQL Injection](https://github.com/swisskyrepo/PayloadsAllTheThings/blob/master/SQL%20Injection/PostgreSQL%20Injection.md)):

| Category | Attack vector | Example payload |
|---|---|---|
| Destructive DDL | DROP/TRUNCATE table, database, schema | `DROP TABLE users;` |
| UNION-based injection | Data exfiltration via UNION SELECT | `' UNION SELECT usename FROM pg_user--` |
| Stacked queries | Injecting extra statements via semicolon | `'; DROP TABLE users;--` |
| Time-based blind | Detecting data via response delay | `SELECT CASE WHEN 1=1 THEN pg_sleep(5) END` |
| Error-based | Extracting data via CAST type errors | `AND 1=CAST((SELECT version()) AS INT)` |
| File read | Reading server files | `SELECT pg_read_file('/etc/passwd')` |
| File write | Writing files to server | `SELECT lo_export(12345, '/tmp/evil')` |
| Command execution | OS command via COPY PROGRAM | `COPY t FROM PROGRAM 'whoami'` |
| C function RCE | Loading libc to execute commands | `CREATE FUNCTION system(cstring) ... LANGUAGE 'c'` |
| XML exfiltration | Dumping data via XML helpers | `SELECT database_to_xml(true,true,'')` |
| Schema enumeration | Querying information\_schema / pg\_shadow | `SELECT * FROM information_schema.tables` |
| Auth bypass | Tautology injection | `' OR '1'='1` |

```
sql_guard {
    mode "blacklist"
    cache yes

    # destructive DDL
    regex "\b(DROP|TRUNCATE)\s+(TABLE|DATABASE|SCHEMA)\b"

    # UNION-based injection
    regex "\bUNION\s+(ALL\s+)?SELECT\b"

    # command execution via COPY PROGRAM
    regex "\bCOPY\s+\w+\s+(FROM|TO)\s+PROGRAM\b"
    regex "\bCOPY\s*\([^)]*\)\s*TO\s+PROGRAM\b"

    # time-based blind injection
    regex "\bpg_sleep\s*\("

    # file read/write
    regex "\bpg_read_file\s*\("
    regex "\blo_import\s*\("
    regex "\blo_export\s*\("
    regex "\blo_from_bytea\s*\("

    # RCE via C language function
    regex "\bCREATE\s+(OR\s+REPLACE\s+)?FUNCTION\b.*\bLANGUAGE\s+'c'"

    # schema enumeration
    regex "\binformation_schema\.(tables|columns|role_table_grants)\b"
    regex "\bpg_shadow\b"

    # XML data exfiltration
    regex "\bdatabase_to_xml\s*\("
    regex "\bquery_to_xml\s*\("

    # authentication bypass
    regex "\b(OR|AND)\s+['0-9].*=\s*['0-9]"

    # stacked query injection
    regex "';\s*(DROP|DELETE|INSERT|UPDATE|CREATE)\b"
}
```

### Whitelist example

Only allow SELECT queries (block everything else):

```
sql_guard {
    mode "whitelist"
    cache yes
    regex "^\s*SELECT\b"
}
```

**Note:** The regex pattern should be tuned to your application. Overly broad patterns
may block legitimate queries (blacklist) or miss dangerous ones (whitelist).
Test thoroughly before deploying in production.

**Performance:** The regex is compiled once at config load time (POSIX ERE,
case-insensitive). With `cache yes`, a hash cache skips the regex for
previously seen queries (~0.07 us per cached lookup vs ~0.25 us for a full
regex match, ~3x speedup). Both blacklist and whitelist modes have identical
performance. In benchmarks with 1M iterations over 20 queries (5 modes), the
cache achieves a 100% hit rate after the initial pass.

To regenerate benchmark results on your hardware:

```
make benchmark-sql-guard
```

Results are saved to `test/benchmark-sql-guard/results.txt`. The CI-repeatable
performance tests are in `test/perf/` and run via `make ci-perf-test`.

---

## **application_name_add_host**

*yes|no*

Add the client host name to the application_name parameter

`application_name_add_host yes`

---

## **reserve_session_server_connection**

*yes|no*

Connect a new client to the server immediately or wait for the first query

`reserve_session_server_connection yes`

---

## **server_lifetime**

*integer*

Server lifetime - maximum number of seconds for a server connection to live. Prevents cache bloat.
Server connection is deallocated only in the idle state.
Defaults to 3600 (1 hour).
Use 0 to disable.

`server_lifetime 3600`

---

## **pool\_client\_idle\_timeout**

*integer*

Client pool idle timeout.

Drop stale client connections after this many seconds of idleness when not in a transaction.

Set to zero to disable.

`pool_client_idle_timeout 0`

---

## **pool\_idle\_in\_transaction\_timeout**

*integer*

Client pool idle in transaction timeout.

Drop client connections in a transaction after this many seconds of idleness.

Set to zero to disable.

`pool_idle_in_transaction_timeout 0`

---

## **pool_reserve_prepared_statement**

*yes|no*

Enable support of prepared statements in transactional pooling.

`pool_reserve_prepared_statement yes`

## **pool_pin_on_listen**
*yes/no*
*Experimental*

Enable pinning client connection to server, after LISTEN command execute.

Default: `no`

`pool_pin_on_listen yes`

---

## **log\_debug**

*yes|no*

Enable verbose mode for a specific route only.

`log_debug no`

---

## **group\_checker\_interval**

*integer*

Soft interval between group checks (in ms)
7000 by default

`group_checker_interval 7000`

---

## **maintain_params**

*yes|no*

User parameters maintenance

By default, Odyssey saves parameter values defined by the user
and deploys them on server attach if they are different from the server's.
This option disables the feature.

`maintain_params no`

---

## **target_session_attrs**

*string*

Target session attributes feature. Odyssey will look up primary/standby
for connections of this user, depending on the value set.
Possible values are:

- read-write - always select host, available for write
- read-only - never select host, available for write
- any (the default) - select host randomly

`target_session_attrs "read-write"`

---

## **quantiles**

*string*

Compute quantiles of query and transaction times

`quantiles "0.99,0.95,0.5"`

---

## **catchup_timeout**

*integer*

Specify maximum replication lag in seconds for the user
See [catchup-timeout.md](../features/catchup-timeout.md) for more details

`catchup_timeout 10`

---

## **catchup_checks**

*integer*

Maximum number of catchup checks before closing 
the connection if the host replication lag is too big
See [catchup-timeout.md](../features/catchup-timeout.md) for more details

`catchup_checks 10`

---

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

---

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
