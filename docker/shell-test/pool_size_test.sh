#!/bin/bash -x
set -e
ody-stop

# We set pool size to 1 and check that 3 clients can do pg_sleep(1) at once.
# We expect them to wait serially on 1 backend.

/usr/bin/odyssey /shell-test/pool_size.conf


for _ in $(seq 1 300); do
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(0.1)' -U user1 -d postgres &
done

for _ in $(seq 1 300); do
  wait -n || {
    code="$?"
    ([[ $code = "127" ]] && exit 0 || exit "$code")
    break
  }
done;

ody-stop