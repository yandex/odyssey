## machinarium

Machinarium allows to create fast networked and event-driven asynchronous applications in
synchronous/procedural manner instead of using traditional callback approach.

The library is based on combination of OS threads (pthreads) and efficient cooperative
multi-tasking primitives (coroutines). Each coroutine executed using own stack context and
transparently scheduled by event loop logic, like `epoll(7)`.

Public API: [sources/machinarium.h](sources/machinarium.h)

### Features overview

#### Machinarium threads and coroutines

#### Messaging and Channels

Machinarium messages and channels are used to provide IPC between threads and
coroutines.

#### Efficient TCP/IP networking

Machinarium IO API primitives can be used to develop high-performance client and server applications.
By doing blocking IO calls, currently run coroutine suspends its execution and switches
to a next one ready to go. When the original request completed, timedout or cancel event happened,
coroutine resumes its execution.

One of the main goals of networking API design is performance. To reduce number of system calls
read operation implemented with readahead support. It is fully buffered and transparently continue to read
socket data even when no active requests are in progress.

Machinarium IO contexts can be transfered between threads, which allows to develop efficient
producer-consumer network applications.

#### DNS resolving

Machinarium implements separate thread-pool to run network address
and service translation functions such as `getaddrinfo()`, to avoid process blocking and to be
consistent with coroutine design.

#### Full-feature SSL/TLS support

#### Timeouts everywhere

#### Cancellation

#### Signal handling
