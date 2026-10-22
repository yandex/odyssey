#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/cancel-3.2/cancel.conf
sleep 1

# background load to ensure no one gets wrong cancel
# Require protocol 3.2: libpq 18 defaults to 3.0 for compatibility.
pgbench 'host=localhost port=6432 user=postgres dbname=postgres min_protocol_version=3.2 max_protocol_version=3.2' -S -T 17 -j2 -c8 --no-vacuum 2>&1 >/tests/cancel-3.2/pgbenchlog &
pgbenchpid=$!

for i in {1..7}; do
	psql 'host=localhost port=6432 user=postgres dbname=postgres min_protocol_version=3.2 max_protocol_version=3.2' -c 'select pg_sleep(10)' 2>/tests/cancel-3.2/log1 1>&2 &
	pid1=$!

	psql 'host=localhost port=6432 user=postgres dbname=postgres min_protocol_version=3.2 max_protocol_version=3.2' -c 'select pg_sleep(10)' 2>/tests/cancel-3.2/log2 1>&2 &
	pid2=$!

	psql 'host=localhost port=6432 user=postgres dbname=postgres min_protocol_version=3.2 max_protocol_version=3.2' -c 'select pg_sleep(10)' 2>/tests/cancel-3.2/log3 1>&2 &
	pid3=$!

	sleep 2

	kill -s SIGINT $pid1
	kill -s SIGINT $pid2
	kill -s SIGINT $pid3

	if wait $pid1; then
		echo "query 1 completed without cancellation"
		exit 1
	else
		cat /tests/cancel-3.2/log1 | grep "ERROR:  canceling statement due to user request" -q || {
			echo "seems like cancel 1 failed"
			cat /tests/cancel-3.2/log1
			exit 1
		}
	fi

	if wait $pid2; then
		echo "query 2 completed without cancellation"
		exit 1
	else
		cat /tests/cancel-3.2/log2 | grep "ERROR:  canceling statement due to user request" -q || {
			echo "seems like cancel 2 failed"
			cat /tests/cancel-3.2/log2
			exit 1
		}
	fi

	if wait $pid3; then
		echo "query 3 completed without cancellation"
		exit 1
	else
		cat /tests/cancel-3.2/log3 | grep "ERROR:  canceling statement due to user request" -q || {
			echo "seems like cancel 3 failed"
			cat /tests/cancel-3.2/log3
			exit 1
		}
	fi
done

wait $pgbenchpid || {
	cat /tests/cancel-3.2/pgbenchlog
	echo "seems like pgbench failed"
	exit 1
}

ody-stop
