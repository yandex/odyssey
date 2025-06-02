#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/unix-socket-storage/conf.conf
sleep 1

pgbench 'host=localhost port=6432 user=postgres dbname=postgres' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 5 || {
	echo "odyssey runned against pg unix socket doesn't work"
	exit 1
}

ody-stop
