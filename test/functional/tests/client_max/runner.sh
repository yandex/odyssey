#!/bin/bash -x

set -ex

ody-start2 /tests/client_max/odyssey.conf postgres postgres

psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select pg_sleep(10000)' &
p=$!
sleep 0.1

if psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select 42'; then
	echo "skibidi"
fi

kill -9 $p
sleep 0.1

psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select 42'

ody-stop
