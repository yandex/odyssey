#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/transactional/odyssey.conf
sleep 1

# ensure route paramaters cached
psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select 42'

# pool_ttl is 5, so the connections used to cache parameters was deallocation
sleep 7

# ensure the parameters connection was deallocated
count=`psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers' -t | sed '/^$/d' | wc -l`
if [ $count -ne 0 ]; then
	echo "expected one server but got" $count
	psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'
	exit 1
fi

# connect, but run the query after 2 seconds
psql 'host=localhost port=6432 user=postgres dbname=postgres' -f '/tests/transactional/f.sql' &
to_kill=$!

sleep 1

# there must be no connections - the query wasnt still sent
count=`psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers' -t | sed '/^$/d' | wc -l`
if [ $count -ne 0 ]; then
	echo "expected one server but got" $count
	psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'
	exit 1
fi

kill -9 $to_kill

ody-stop
