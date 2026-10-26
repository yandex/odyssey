#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/cancel/cancel.conf
sleep 1

# background load to ensure no one gets wrong cancel
pgbench 'host=localhost port=6432 user=postgres dbname=postgres' -S -T 17 -j2 -c8 --no-vacuum 2>&1 >/tests/cancel/pgbenchlog &
pgbenchpid=$!

wait_for_cancellation() {
	local pid=$1
	local log=$2
	local exit_code=0
	wait "$pid" || exit_code=$?
	if [ "$exit_code" -eq 0 ] || ! grep -Fq 'ERROR:  canceling statement due to user request' "$log"; then
		echo "Expected query cancellation, got psql exit code $exit_code: $log" >&2
		cat "$log" >&2
		return 1
	fi
}

for i in {1..7}; do
	psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select pg_sleep(10)' 2>/tests/cancel/log1 1>&2 &
	pid1=$!

	psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select pg_sleep(10)' 2>/tests/cancel/log2 1>&2 &
	pid2=$!

	psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select pg_sleep(10)' 2>/tests/cancel/log3 1>&2 &
	pid3=$!

	sleep 2

	kill -s SIGINT $pid1
	kill -s SIGINT $pid2
	kill -s SIGINT $pid3

	wait_for_cancellation "$pid1" /tests/cancel/log1
	wait_for_cancellation "$pid2" /tests/cancel/log2
	wait_for_cancellation "$pid3" /tests/cancel/log3
done

wait $pgbenchpid || {
	cat /tests/cancel/pgbenchlog
	echo "seems like pgbench failed"
	exit 1
}

ody-stop
