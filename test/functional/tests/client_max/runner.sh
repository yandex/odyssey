#!/bin/bash -x

set -ex

ody-start2 /tests/client_max/odyssey.conf postgres postgres

psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select pg_sleep(10000)' &
p=$!
sleep 0.1

if psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select 42'; then
	echo "the connect must fail"
	exit 1
fi

# reserve_clients for listen allows to add some connections
psql 'host=localhost port=9999 user=postgres dbname=postgres sslmode=disable' -c 'select pg_sleep(10000)' &
p1=$!
sleep 0.5

psql 'host=localhost port=9999 user=postgres dbname=postgres sslmode=disable' -c 'select pg_sleep(10000)' &
p2=$!
sleep 0.5

if psql 'host=localhost port=9999 user=postgres dbname=postgres sslmode=disable' -c 'select 42'; then
	echo "the connect must fail"
	exit 1
fi

kill -9 $p2
kill -9 $p1

kill -9 $p
sleep 0.1

psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select 42'
psql 'host=localhost port=9999 user=postgres dbname=postgres sslmode=disable' -c 'select 42'

ody-stop
