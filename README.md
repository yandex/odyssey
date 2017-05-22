**machinarium**

Machinarium allows to create fast networked and event-driven asynchronous applications<br>
in synchronous/procedural manner instead of using traditional callback approach.

The library is based on using and combining pthreads(7) and efficient<br>
cooperative multi-tasking primitives (coroutines). Each coroutine executed using own stack<br>
and scheduling is transparently driven by epoll(2).
