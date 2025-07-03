#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/cancel/cancel.conf
sleep 1

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

wait $pid1 || {
	cat /tests/cancel/log1 | grep "ERROR:  canceling statement due to user request" -q || {
		echo "seems like cancel 1 failed"
		cat /tests/cancel/log1
		exit 1
	}
}

wait $pid2 || {
	cat /tests/cancel/log2 | grep "ERROR:  canceling statement due to user request" -q || {
		echo "seems like cancel 2 failed"
		cat /tests/cancel/log2
		exit 1
	}
}

wait $pid3 || {
	cat /tests/cancel/log3 | grep "ERROR:  canceling statement due to user request" -q || {
		echo "seems like cancel 3 failed"
		cat /tests/cancel/log3
		exit 1
	}
}

ody-stop
