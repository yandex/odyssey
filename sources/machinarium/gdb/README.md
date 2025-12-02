# GDB scripts for machinarium easy debugging

## How to use

Run gdb and then initialize the script:
```bash
(gdb) source ~/odyssey/sources/machinarium/gdb/machinarium-gdb.py
```

## Commands

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
Also note, that in mostly cases there will be zero-frame, which is fake (this is just how gdb's unwinders works)

See `help mmcoro` for more.

Ex:
```
(gdb) mmcoro 2 0 bt
#1  0x000055555555dd1f in mm_scheduler_yield (scheduler=0x55555557c828) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:166
#2  0x000055555555e106 in mm_call (call=0x7ffff7fb5e08, type=MM_CALL_EVENT, time_ms=4294967295) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/call.c:60
#3  0x000055555555ef3c in mm_eventmgr_wait (mgr=0x55555557c928, event=0x7ffff7fb5e00, time_ms=4294967295)
    at /home/rkhapov/odyssey/build/third_party/machinarium/sources/event_mgr.c:101
#4  0x000055555555fc76 in mm_channel_read (channel=0x5555555670f0 <machinarium+80>, time_ms=4294967295)
    at /home/rkhapov/odyssey/build/third_party/machinarium/sources/channel.c:117
#5  0x00005555555598cf in mm_taskmgr_main (arg=0x0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/task_mgr.c:20
#6  0x000055555555d6fa in mm_scheduler_main (arg=0x7ffff0000b70) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:17
#7  0x00005555555609ab in mm_context_runner () at /home/rkhapov/odyssey/build/third_party/machinarium/sources/context.c:28
#8  0x0000000000000000 in ?? ()

(gdb) mmcoro 4 0 frame apply 7 info locals
Please be careful with analyzing for the frame with zero index
Unless your command is not bt, the zero frame will be printed, and this frame is fake.

#0  mm_clock_cmp (a=0x0, b=0x0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/clock.c:13
__PRETTY_FUNCTION__ = "mm_clock_cmp"
#1  0x000055555555dd1f in mm_scheduler_yield (scheduler=0x55555557d898) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:166
current = 0x7fffe0000b70
resume = 0x55555557d8a0
__PRETTY_FUNCTION__ = "mm_scheduler_yield"
#2  0x000055555555e106 in mm_call (call=0x7ffff7ef4ec0, type=MM_CALL_SLEEP, time_ms=0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/call.c:60
scheduler = 0x55555557d898
clock = 0x55555557da50
coroutine = 0x7fffe0000b70
#3  0x0000555555558a03 in machine_sleep (time_ms=0) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/machine.c:201
call = {type = MM_CALL_SLEEP, coroutine = 0x7fffe0000b70, timer = {active = 1, timeout = 181455347, interval = 0, seq = 248, callback = 0x55555555dece <mm_call_timer_cb>, arg = 0x7ffff7ef4ec0, clock = 0x55555557da50}, cancel_function = 0x55555555df3f <mm_call_cancel_cb>, arg = 0x7ffff7ef4ec0, data = 0x0, timedout = 0, status = 0}
#4  0x00005555555580b7 in do_smt2 (arg=0x0) at benchmark_csw.c:71
c = 103 'g'
#5  0x000055555555810e in do_smt (arg=0x0) at benchmark_csw.c:79
No locals.
#6  0x000055555555d6fa in mm_scheduler_main (arg=0x7fffe0000b70) at /home/rkhapov/odyssey/build/third_party/machinarium/sources/scheduler.c:17
coroutine = 0x7fffe0000b70
scheduler = 0x55555557d898
i = 0x0
```