<p align="center">
	<a href=""><img src="documentation/odyssey.png" width="35%" height="35%" /></a><br>
</p>
<br>

## Odyssey

Advanced multi-threaded PostgreSQL connection pooler and request router.

### Design goals and main features

#### Multi-threaded processing

Odyssey can significantly scale processing performance by
specifying a number of additional worker threads. Each worker thread is
responsible for authentication and proxying client-to-server and server-to-client
requests. All worker threads are sharing global server connection pools.
Multi-threaded design plays important role in `SSL/TLS` performance.

#### Advanced transactional pooling

Odyssey tracks current transaction state and in case of unexpected client
disconnection can emit automatic `Cancel` connection and do `Rollback` of
abandoned transaction, before putting server connection back to
the server pool for reuse. Additionally, last server connection owner client
is remembered to reduce a need for setting up client options on each
client-to-server assignment.

#### Better pooling control

Odyssey allows to define connection pools as a pair of `Database` and `User`.
Each defined pool can have separate authentication, pooling mode and limits settings.

#### Authentication

Odyssey has full-featured `SSL/TLS` support and common authentication methods
like: `md5` and `clear text` both for client and server authentication.
Additionally it allows to block each pool user separately.

#### Logging

Odyssey generates universally unique identifiers `uuid` for client and server connections.
Any log events and client error responces include the id, which then can be used to
uniquely identify client and track actions. Odyssey can save log events into log file and
using system logger.

#### Architecture and internals

Odyssey has sophisticated asynchronous multi-threaded architecture which
is driven by custom made coroutine engine: [machinarium](https://github.yandex-team.ru/pmwkaa/machinarium).
Main idea behind coroutine design is to make event-driven asynchronous applications to look and feel
like being written in synchronous-procedural manner instead of using traditional
callback approach.

One of the main goal was to make code base understandable for new developers and
to make an architecture easily extensible for future development.

More information: [Architecture and internals](documentation/internals.md).

### Build instructions

```sh
git clone --recursive git://github.yandex-team.ru/pmwkaa/odyssey.git
cd odyssey
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
