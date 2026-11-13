#!/bin/bash -x

set -ex

# pool_timeout bounds the whole wait for a server, including the retry after
# the backend rejects a new connection with "too many connections".

PG='host=localhost port=5432 user=postgres dbname=postgres sslmode=disable'
DSN='host=localhost port=6432 user=user1 dbname=db1 sslmode=disable'

cleanup() {
	kill $(jobs -p) 2>/dev/null || true
	wait || true
	psql "$PG" -c 'alter role user1 connection limit 1000'
}
trap cleanup EXIT

psql "$PG" -c 'alter role user1 connection limit 2'

ody-start /tests/pool_timeout_retry/odyssey.conf user1 db1

psql "$DSN" -c 'select pg_sleep(30)' &
psql "$DSN" -c 'select pg_sleep(30)' &
sleep 1

start_ms=$(date +%s%3N)
psql "$DSN" -c 'select 1' &
waiter=$!

# The waiter queues behind the full pool. Free a pool slot 2 s in, but lower
# the role limit first, so the backend rejects the waiter's new connection and
# the waiter has to wait for an idle server with what is left of pool_timeout.
sleep 2
psql "$PG" -c 'alter role user1 connection limit 1'
psql "$PG" -c "select pg_terminate_backend(pid) from pg_stat_activity where usename = 'user1' and query = 'select pg_sleep(30)' limit 1"

if wait $waiter; then
	echo "the waiter must time out"
	exit 1
fi
elapsed_ms=$(( $(date +%s%3N) - start_ms ))

grep -q 'too many connections for role' /var/log/odyssey.log || {
	echo "the backend must reject the waiter's connection"
	cat /var/log/odyssey.log
	exit 1
}

# about 3000 ms with one deadline, about 5000 ms if the retry restarts it
if [ $elapsed_ms -ge 4500 ]; then
	echo "waited ${elapsed_ms} ms, pool_timeout is 3000 ms"
	exit 1
fi

cleanup
ody-stop
