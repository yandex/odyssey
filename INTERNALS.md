
### Odissey architecture and internals

Odissey heavily depends on two libraries, which were originally created during its
development: Machinarium and Shapito.

**Machinarium**

Machinarium extensively used for organization of multi-thread processing, cooperative multi-tasking
and networking IO. All Odissey threads are run in context of machinarium `machines` -
pthreads with coroutine schedulers placed on top of `epoll(2)` event loop.

Odissey does not directly use or create multi-tasking primitives such as OS threads and mutexes.
All synchronization is done using message passing and transparently handled by machinarium.

Repository: [github/machinarium](https://github.yandex-team.ru/pmwkaa/machinarium)

**Shapito**

Shapito provides resizable buffers (streams) and methods for constructing, reading and validating
PostgreSQL protocol requests. By design, all PostgreSQL specific details should be provided by
Shapito library.

Repository: [github/shapito](https://github.yandex-team.ru/pmwkaa/shapito).

#### Core components

```
                          main()
                       ┌──────────┐
                       │ instance │
        thread         └──────────┘
      ┌────────┐                       ┌────────────┐
      │ pooler │                       │ relay_pool │
      └────────┘                       └────────────┘
          ┌─────────┐           ┌────────┐         ┌────────┐
          │ servers │           │ relay0 │   ...   │ relayN │
          └─────────┘           └────────┘         └────────┘
          ┌────────┐              thread             thread
          │ router │
          └────────┘
          ┌──────────┐
          │ periodic │
          └──────────┘
          ┌─────────┐
          │ console │
          └─────────┘

```

**Instance**
**Pooler**
**Servers**
**Router**
**Periodic**
**Console and signal handling**
**Relay pool**
