#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/ignore-cancel/ignore-cancel.conf
sleep 1

for i in {1..3}; do
	psql 'host=localhost port=6432 user=postgres dbname=postgres' \
		-c 'select pg_sleep(3)' 2>/tests/ignore-cancel/log$i 1>&2 &
	pid=$!

	sleep 1

	# psql sends a cancel request on SIGINT; with ignore_cancel enabled
	# odyssey must drop it: no "canceling statement" error must reach
	# the client and the query must run to completion
	kill -s SIGINT $pid

	wait $pid || {
		if grep "ERROR:  canceling statement due to user request" /tests/ignore-cancel/log$i -q; then
			echo "seems like cancel $i was not ignored"
			cat /tests/ignore-cancel/log$i
			exit 1
		fi
	}

	grep "Cancel request sent" /tests/ignore-cancel/log$i -q || {
		echo "seems like cancel request $i was not sent"
		cat /tests/ignore-cancel/log$i
		exit 1
	}
done

for i in {1..10}; do
	grep "cancel request ignored" /var/log/odyssey.log -q && break
	sleep 1
done

grep "cancel request ignored" /var/log/odyssey.log -q || {
	echo "no 'cancel request ignored' message in odyssey log"
	tail -50 /var/log/odyssey.log
	exit 1
}

ody-stop
