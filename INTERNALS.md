
### Odissey architecture and internals

Odissey heavily depends on two libraries, which were originally created during its
development: Machinarium and Shapito.

#### Machinarium

Machinarium extensively used for organization of multi-thread processing, cooperative multi-tasking
and networking IO. All Odissey threads are run in context of machinarium `machines` -
pthreads with coroutine schedulers placed on top of `epoll(7)` event loop.

Odissey does not directly use or create multi-tasking primitives such as OS threads and mutexes.
All synchronization is done using message passing and transparently handled by machinarium.

Repository: [github/machinarium](https://github.yandex-team.ru/pmwkaa/machinarium)

#### Shapito

Shapito provides resizable buffers (streams) and methods for constructing, reading and validating
PostgreSQL protocol requests. By design, all PostgreSQL specific details should be provided by
Shapito library.

Repository: [github/shapito](https://github.yandex-team.ru/pmwkaa/shapito).

#### Core components

```
                                              main()
                                           .----------.
                                           | instance |
                           thread          '----------'
                         .--------.                          .------------.
                         | pooler |                          | relay_pool |
                         '--------'                          '------------'
                  .--------.    .---------.           .--------.         .--------.
                  | router |    | servers |           | relay0 |   ...   | relayN |
                  '--------'    '---------'           '--------'         '--------'
                  .---------.   .----------.            thread             thread
                  | console |   | periodic |
                  '---------'   '----------'
```

#### Instance

Application entry point.

Handle initialization. Read configuration file, prepare loggers.
Run pooler and relay\_pool threads.

[sources/instance.h](sources/instance.h), [sources/instance.c](sources/instance.c)

#### Pooler

Start router, periodic and console subsystems.

Create listen server one for each resolved address. Each listen server runs inside own coroutine.
Server coroutine mostly waits on `machine_accept()`.

On incoming connection, new client context is created and notification message is sent to next
relay worker using `relaypool_feed()`. Client IO context is detached from pooler `epoll(7)` context.

Handle signals using `machine_signal_wait()`. On `SIGHUP`: do versional config reload, add new databases
and obsolete old ones. On `SIGINT`: call `exit(3)`. Other threads are blocked from receiving signals.

[sources/pooler.h](sources/pooler.h), [sources/pooler.c](sources/pooler.c)

#### Router

Handle client registration and routing requests. Do client-to-server attachment and detachment.
Ensure connection limits and client pool queueing. Handle implicit `Cancel` client request, since access
to server pool is required to match a client key.

Router works in request-reply manner: client (from relay thread) sends a request message to
router and waits for reply. Could be a potential hot spot (not an issue at the moment).

[sources/router.h](sources/router.h), [sources/router.c](sources/router.c)

#### Periodic

Do periodic service tasks, like idle server connection expiration and
database scheme obsoletion.

[sources/periodic.h](sources/periodic.h), [sources/periodic.c](sources/periodic.c)

#### Relay and Relay pool

Relay machine (thread) waits on incoming connection notification queue. On new connection event,
create new frontend coroutine and handle client (frontend) lifecycle. Each relay thread can host
thousands of client coroutines.

Relay pool is responsible for maintaining a worker thread pool. Threads are machinarium machines,
created using `machine_create()`.

[sources/relay.h](sources/relay.h), [sources/relay.c](sources/relay.c),
[sources/relay_pool.h](sources/relay_pool.h), [sources/relay_pool.c](sources/relay_pool.c)

### Client (frontend) lifecycle

Client logic is driven by `od_frontend()` function, which is a coroutine entry point.

Client processing stages:

#### 1. Startup
#### 2. Process Cancel request
#### 3. Route client
#### 4. Authenticate client
#### 5. main
#### 6. Cleanup
