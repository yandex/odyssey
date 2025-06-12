#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/pool_timeout_0/odyssey.conf
sleep 1

pgbench 'host=localhost port=6432 user=postgres dbname=postgres' -j 2 -c 51 --select-only --no-vacuum --progress 1 -T 5 --max-tries=1 && {
	cat /var/log/odyssey.log
	echo "pool size is 50 and there is 51 clients, this should not work"
	exit 1
}

pgbench 'host=localhost port=6432 user=postgres dbname=postgres' -j 2 -c 50 --select-only --no-vacuum --progress 1 -T 5 --max-tries=1 || {
	cat /var/log/odyssey.log
	echo "pool size is 50 and there is 50 clients, this should work"
	exit 1
}

ody-stop
