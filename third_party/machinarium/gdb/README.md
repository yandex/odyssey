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
(gdb) mmcoro t=3,id=1 info stack
#0  bar (arg=0x0) at benchmark_csw.c:23
#1  0x0000555555557f43 in foo (arg=0x0) at benchmark_csw.c:31
#2  0x0000555555557f8e in benchmark_worker (arg=0x0) at benchmark_csw.c:39
#3  0x000055555555d657 in mm_scheduler_main (arg=0x7fffe8001010) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:17
#4  0x0000555555560908 in mm_context_runner () at /home/rkhapov/odyssey/build/third_party/machinarium/sources/context.c:28
#5  0x0000000000000000 in ?? ()

(gdb) mmcoro t=3,id=1 t=3,id=0 info stack
#0  bar (arg=0x0) at benchmark_csw.c:23
#1  0x0000555555557f43 in foo (arg=0x0) at benchmark_csw.c:31
#2  0x0000555555557f8e in benchmark_worker (arg=0x0) at benchmark_csw.c:39
#3  0x000055555555d657 in mm_scheduler_main (arg=0x7fffe8001010) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:17
#4  0x0000555555560908 in mm_context_runner () at /home/rkhapov/odyssey/build/third_party/machinarium/sources/context.c:28
#5  0x0000000000000000 in ?? ()
#0  mm_scheduler_yield (scheduler=0x0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:167
#1  0x0000555555557f43 in foo (arg=0x0) at benchmark_csw.c:31
#2  0x0000555555557f8e in benchmark_worker (arg=0x0) at benchmark_csw.c:39
#3  0x000055555555d657 in mm_scheduler_main (arg=0x7fffe8001010) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:17
#4  0x0000555555560908 in mm_context_runner () at /home/rkhapov/odyssey/build/third_party/machinarium/sources/context.c:28
#5  0x0000000000000000 in ?? ()

(gdb) mmcoro t=3,id=1 id=0,thread=3 thr=2,id=0 info stack
#0  bar (arg=0x0) at benchmark_csw.c:23
#1  0x0000555555557f43 in foo (arg=0x0) at benchmark_csw.c:31
#2  0x0000555555557f8e in benchmark_worker (arg=0x0) at benchmark_csw.c:39
#3  0x000055555555d657 in mm_scheduler_main (arg=0x7fffe8001010) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:17
#4  0x0000555555560908 in mm_context_runner () at /home/rkhapov/odyssey/build/third_party/machinarium/sources/context.c:28
#5  0x0000000000000000 in ?? ()
#0  mm_scheduler_yield (scheduler=0x0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:167
#1  0x0000555555557f43 in foo (arg=0x0) at benchmark_csw.c:31
#2  0x0000555555557f8e in benchmark_worker (arg=0x0) at benchmark_csw.c:39
#3  0x000055555555d657 in mm_scheduler_main (arg=0x7fffe8001010) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:17
#4  0x0000555555560908 in mm_context_runner () at /home/rkhapov/odyssey/build/third_party/machinarium/sources/context.c:28
#5  0x0000000000000000 in ?? ()
#0  mm_scheduler_yield (scheduler=0x555555582390) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:167
#1  0x000055555555be17 in mm_loop_step (loop=0x55555557c9e0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/loop.c:64
#2  0x00005555555583b1 in machine_main (arg=0x55555557c7e0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/machine.c:56
#3  0x00007ffff7694ac3 in start_thread (arg=<optimized out>) at ./nptl/pthread_create.c:442
#4  0x00007ffff7726850 in clone3 () at ../sysdeps/unix/sysv/linux/x86_64/clone3.S:81
```
