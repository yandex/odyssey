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
