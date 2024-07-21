# GDB scripts for machinarium easy debugging

## How to use

Run gdb and then initialize the script:
```bash
(gdb) source ~/odyssey/third_party/machinarium/gdb/machinarium-gdb.py
```

## Commnands

### info mmcoros
List all machinarium coroutines for threads.
Shows list of active and ready coroutines from scheduler of the thread.
Current coroutine are marked by *.
See `(gdb) help info mmcoros` for more.

Ex:
```
(gdb) info mmcoros
Thread 3 (benchmark_csw) machinarium coroutines:
 Id	State		errno	Function
 0	MM_CACTIVE	0	0x555555557fe0 <benchmark_runner>
*1	MM_CACTIVE	0	0x555555557f70 <benchmark_worker>

Thread 2 (resolver: 0) machinarium coroutines:
 Id	State		errno	Function
 0	MM_CACTIVE	0	0x5555555596e3 <mm_taskmgr_main>

Thread 1 (benchmark_csw) machinarium coroutines:
 The mm_self is NULL, so no coroutines in this thread available.

(gdb) info mmcoros 'benchmark_csw'
Thread 3 (benchmark_csw) machinarium coroutines:
 Id	State		errno	Function
 0	MM_CACTIVE	0	0x555555557fe0 <benchmark_runner>
*1	MM_CACTIVE	0	0x555555557f70 <benchmark_worker>

Thread 1 (benchmark_csw) machinarium coroutines:
 The mm_self is NULL, so no coroutines in this thread available.

(gdb) info mmcoros 2 3
Thread 3 (benchmark_csw) machinarium coroutines:
 Id	State		errno	Function
 0	MM_CACTIVE	0	0x555555557fe0 <benchmark_runner>
*1	MM_CACTIVE	0	0x555555557f70 <benchmark_worker>

Thread 2 (resolver: 0) machinarium coroutines:
 Id	State		errno	Function
 0	MM_CACTIVE	0	0x5555555596e3 <mm_taskmgr_main>

```

### mmcoro

Execute gdb command in context of the specified coroutine.

Please note, that coroutine is determined with pair thread id and coroutine id.

See `help mmcoro` for more.

Ex:
```
(gdb) mmcoro 3 0 info stack
#0  mm_scheduler_yield (scheduler=0x0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:167
#1  0x0000555555557f43 in foo (arg=0x0) at benchmark_csw.c:31
#2  0x0000555555557f8e in benchmark_worker (arg=0x0) at benchmark_csw.c:39
#3  0x000055555555d657 in mm_scheduler_main (arg=0x7fffe8001010) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:17
#4  0x0000555555560908 in mm_context_runner () at /home/rkhapov/odyssey/build/third_party/machinarium/sources/context.c:28
#5  0x0000000000000000 in ?? ()

(gdb) mmcoro 3 0 frame apply 3 info locals
#0  mm_scheduler_yield (scheduler=0x0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:167
current = 0x55555556101d
resume = 0x5d0f7680faa
__PRETTY_FUNCTION__ = "mm_scheduler_yield"
#1  0x0000555555557f43 in foo (arg=0x0) at benchmark_csw.c:31
foo_var = 52
#2  0x0000555555557f8e in benchmark_worker (arg=0x0) at benchmark_csw.c:39
bench_worker_var = 99 'c'

(gdb) mmcoro 3 0
$1 = {id = 0, state = MM_CACTIVE, cancel = 0, errno_ = 0, function = 0x55555555803b <benchmark_runner>, function_arg = 0x0, stack = {pointer = 0x7ffff7fa8000 "", 
    size = 16384, size_guard = 4096}, context = {sp = 0x7ffff7fabdf8}, resume = 0x55555557cca0, call_ptr = 0x7ffff7fabeb0, joiners = {next = 0x7fffe8000bc8, 
    prev = 0x7fffe8000bc8}, link_join = {next = 0x7fffe8000bd8, prev = 0x7fffe8000bd8}, link = {next = 0x7fffe8001088, prev = 0x55555557cd40}}
```