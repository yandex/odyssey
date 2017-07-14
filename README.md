
## Odissey

Advanced multi-threaded PostgreSQL connection pooler and request router.

### Design goals and main features

**Multi-threaded architecture**

Odissey can significantly scale its processing performance by
specifying a number of additional worker threads. Each worker thread is
responsible for authentication and proxying client-to-server and server-to-client
requests. All worker threads are sharing global server connection pools.
Multi-threaded design plays important role in SSL/TLS performance.

**Advanced transactional pooling**

Odissey tracks current transaction state and in case of unexpected client
disconnection can emit automatic `Cancel` connection and do `Rollback` of
abandoned transaction, before putting server connection back to
the server pool for reuse. Additionally, last server connection owner client
is remembered to reduce a need for `Discard` and `Setting` up client options
on each client-to-server assignment.

**Better pooling control**

Odissey allows to define connection pools as a pair of `database name` and `user`.
Each defined pool can have its own authentication, pooling mode and limits settings.

**Pipelining and network optimizations**

Odissey allows to reduce network IO calls by logically buffer several
server replies before sending it to the client. Additionally
client-to-server and server-to-client buffers are zero-copy.

**Authentication**

Odissey has full-featured SSL/TLS support and common authentication methods
like: `md5` and `clear text` both for client and server authentication.
Additionally it allows to block each pool user separately.

**Internals**

Odissey has sophisticated asynchonous multi-threaded architecture which
is driven by custom made coroutine engine: [machinarium](https://github.yandex-team.ru/pmwkaa/machinarium).
Main idea behind coroutines design is to make event-driven asynchronous applications in
synchronous-procedural look and feel instead of using traditional
callback approach.

Notes on architecture and internal design: [sources/README.md](sources/README.md).

### Build instructions

```sh
git clone --recursive git://github.yandex-team.ru/pmwkaa/odissey.git
cd odissey
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
