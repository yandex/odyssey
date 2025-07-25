# Welcome to Odyssey

## What is Odyssey

Advanced multi-threaded PostgreSQL connection pooler and request router.

Odyssey is production-ready, it is being used in large production setups.
We appreciate any kind of feedback and contribution to the project.

## Why Odyssey

### Multi-threaded processing

Odyssey can significantly scale processing performance by
specifying a number of additional worker threads. Each worker thread is
responsible for authentication and proxying client-to-server and server-to-client
requests. All worker threads are sharing global server connection pools.
Multi-threaded design plays important role in `SSL/TLS` performance.

### Advanced transactional pooling

Odyssey tracks current transaction state and in case of unexpected client
disconnection can emit automatic `Cancel` connection and do `Rollback` of
abandoned transaction, before putting server connection back to
the server pool for reuse. Additionally, last server connection owner client
is remembered to reduce a need for setting up client options on each
client-to-server assignment.

### Better pooling control

Odyssey allows to define connection pools as a pair of `Database` and `User`.
Each defined pool can have separate authentication, pooling mode and limits settings.

### Authentication

Odyssey has full-featured `SSL/TLS` support and common authentication methods
like: `md5` and `clear text` both for client and server authentication. 
Odyssey supports PAM & LDAP authentication, this methods operates similarly to `clear text` auth except that it uses 
PAM/LDAP to validate user name/password pairs. PAM optionally checks the connected remote host name or IP address.
Additionally it allows to block each pool user separately.
