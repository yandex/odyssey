# bug-13 deterministic repro

Race in `od_global_pstmt_try_remove()` (pstmt.c): two threads may
unref the same global prepared statement down to `refs == 1` at the
same time and both enter `try_remove`. The winner removes and frees the
hashmap entry; the loser then hashes the *freed* key (`xxh_pstmt_desc`,
heap-use-after-free) and/or hits `od_assert(klock.kvp != NULL)` /
`rc == -1` is impossible for a no-create `mm_hashmap_lock_key`, so the
NULL kvp is never detected gracefully.

`ODyssey_pstmt_try_remove` injection point (armed via the console
command `LOAD pstmt_try_remove`) parks every racer right before
`mm_hashmap_lock_key`, so the race is 100% deterministic instead of
1:110k fuzzing iterations.

## Requirements

- odyssey built from this branch:
  `cmake -B build -DCMAKE_BUILD_TYPE=ASAN && make -C build -j8`
- a PostgreSQL reachable at 127.0.0.1:17302 (trust auth) with
  databases `postgres` and `simplex`:
  `createdb -h 127.0.0.1 -p 17302 -U postgres simplex`
- the config from this dir: `odyssey.conf`

## Run

```sh
: > /tmp/bug13-odyssey.log
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
    setsid odyssey/build/sources/odyssey tests/bug13/odyssey.conf \
    > /tmp/bug13-odyssey.log 2>&1 < /dev/null &
sleep 3

OD_LOG=/tmp/bug13-odyssey.log python3 tests/bug13/test_inject.py
```

Expected output:

```
== A(postgres): Parse s1 + Sync, then RST A
== console: arm injection point
== console: DROP SERVERS -> unref serverA (v=2) -> PARK #1
== B(simplex): Parse s1 + Sync, then RST B
== console: DROP SERVERS -> unref serverB (v=2) -> PARK #2
== console: release -> both race for the bucket lock
CRASH REPRODUCED: odyssey dead
```

and in the log:

```
==NNN==ERROR: AddressSanitizer: heap-use-after-free
    #2 xxh_pstmt_desc            sources/pstmt.c:590
    #3 mm_hashmap_lock_key      sources/machinarium/ds/hm.c:324
    #4 od_global_pstmt_try_remove  sources/pstmt.c:747
```

(the assert flavor `od_assert(klock.kvp != NULL)` fires when the freed
chunk hash lookup happens to return rc == 0 / kvp == NULL instead)

## How it works

1. Each client does `Parse("s1", "select 1") + Sync` and drops the
   connection (RST). After the teardown the only holders of the global
   entry are the map itself and the server's prepared-statement slot
   (pool_reserve_prepared_statement yes keeps it alive).
2. `DROP SERVERS` kicks the pool cleanup: the server slot unref brings
   refs to 1 and `od_global_pstmt_try_remove` is entered - and parked
   by the injection point (PARK #1).
3. The second client uses the *other* route (`simplex`), so its server
   cleanup runs in parallel and yields PARK #2. With a single route
   the cleanup coroutine is parked holding the route lock and the
   second cleanup never happens (deadlock by design).
4. The second `LOAD pstmt_try_remove` releases the tokens; both waiters
   wakeup and race for the bucket lock: winner removes and frees the
   entry, loser dereferences the freed key.
