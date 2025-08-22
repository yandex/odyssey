<p align="center">
	<img src="docs/img/odyssey.png" width="35%" height="35%" /><br>
</p>
<br>

## Odyssey

AI-ready multi-threaded PostgreSQL connection pooler and request router.

#### Project status

Odyssey is production-ready, it is being used in large production setups. We appreciate any kind of feedback and contribution to the project.

<a href="https://scan.coverity.com/projects/yandex-odyssey">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/20374/badge.svg"/>
</a>

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
Odyssey supports PAM & LDAP authentication, this methods operates similarly to `clear text` auth except that it uses 
PAM/LDAP to validate user name/password pairs. PAM optionally checks the connected remote host name or IP address.
Additionally it allows to block each pool user separately.

#### Logging

Odyssey generates universally unique identifiers `uuid` for client and server connections.
Any log events and client error responses include the id, which then can be used to
uniquely identify client and track actions. Odyssey can save log events into log file and
using system logger.


#### Build instructions

Currently Odyssey runs only on Linux. Supported platforms are x86/x86_64.

To build you will need in ubuntu distros:

* cmake >= 3.12.4
* gcc >= 4.6
* openssl
* postgresql-server-dev-13
* pg_config utility is in the PATH

And for fedora-based distros:

* postgresql-static
* postgresql-server-devel
* postgresql-private-libs
* postgresql-private-devel
* openssl-devel

```sh
git clone git://github.com/yandex/odyssey.git
cd odyssey
make local_build
```
Adapt odyssey-dev.conf then:
```sh
make local_run
```

Alternatively:
```sh
make console_run
```

### Documentation

See [docs](docs/) dir with more documentation info. You can serve docs locally by `make serve_docs`.

### Support

We have a [Telegram chat](https://t.me/+ecwqGEkVgXg2OTQy) to discuss Odyssey usage and development.
