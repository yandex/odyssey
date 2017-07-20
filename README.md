## Machinarium

Machinarium allows to create fast networked and event-driven asynchronous applications in
synchronous/procedural manner instead of using traditional callback approach.

*Public API*  [sources/machinarium.h](sources/machinarium.h)

#### Threads and coroutines

Machinarium is based on combination of `pthreads(7)` and custom made implementation of efficient cooperative
multi-tasking primitives (coroutines).

Each coroutine executed using own stack context and transparently scheduled by `epoll(7)` event-loop logic.
Each working machinarium thread can handle thousands of executing coroutines.

#### Messaging and Channels

Machinarium messages and channels are used to provide IPC between threads and
coroutines. Ideally, this approach should be sufficient to fulfil needs of most multi-threaded applications
without the need of using additional access synchronization.

#### Efficient TCP/IP networking

Machinarium IO API primitives can be used to develop high-performance client and server applications.
By doing blocking IO calls, currently run coroutine suspends its execution and switches
to a next one ready to go. When the original request completed, timedout or cancel event happened,
coroutine resumes its execution.

One of the main goals of networking API design is performance. To reduce number of system calls
read operation implemented with readahead support. It is fully buffered and transparently continue to read
socket data even when no active calls are in progress.

Machinarium IO contexts can be transferred between threads, which allows to develop efficient
producer-consumer network applications.

#### DNS resolving

Machinarium implements separate thread-pool to run network address
and service translation functions such as `getaddrinfo()`, to avoid process blocking and to be
consistent with coroutine design.

#### Full-feature SSL/TLS support

Machinarium has easy-to-use Transport Layer Security (TLS) API on board, which also support work
with multiple threads. Create and configure machinarium TLS object, associate it with any existing
IO context and it will be automatically upgraded to SSL/TLS usage.

#### Timeouts everywhere

#### Cancellation

#### Signal handling
