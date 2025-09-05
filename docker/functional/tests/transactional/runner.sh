#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/transactional/odyssey.conf
sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres' -f '/tests/transactional/f.sql' &
to_kill=$!

sleep 1

count=`psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers' -t | sed '/^$/d' | wc -l`

if [ $count -ne 0 ]; then
	echo "expected one server but got" $count
	psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'
	exit 1
fi

kill -9 $to_kill

ody-stop
