#!/bin/bash

set -ex

/usr/bin/odyssey /tests/tsa_ports/odyssey.conf
sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select pg_is_in_recovery()' | grep 't' || {
    echo 'exptected to port 6432 return connect to replica'
    exit 1
}

psql 'host=localhost port=6433 user=postgres dbname=postgres' -c 'select pg_is_in_recovery()' | grep 'f' || {
    echo 'exptected to port 6433 return connect to master'
    exit 1
}

psql 'host=localhost port=6433 user=postgres dbname=postgres' -f /tests/tsa_ports/create_check_fn.sql

# limits are 50 but we an use up to 100 connections, 50 per each endpoint
pgbench 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -j 2 -c 50 -f /tests/tsa_ports/check_is_replica.sql --no-vacuum --progress 1 -T 5 --max-tries=1 &
pgbench 'host=localhost port=6433 user=postgres dbname=postgres sslmode=disable' -j 2 -c 50 -f /tests/tsa_ports/check_is_master.sql --no-vacuum --progress 1 -T 5 --max-tries=1 &

wait -n || {
    cat /var/log/odyssey.log
    exit 1
}

wait -n || {
    cat /var/log/odyssey.log
    exit 1
}

pgbench 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -j 2 -c 50 -f /tests/tsa_ports/check_is_replica.sql --connect --no-vacuum --progress 1 -T 5 --max-tries=1 &
pgbench 'host=localhost port=6433 user=postgres dbname=postgres sslmode=disable' -j 2 -c 50 -f /tests/tsa_ports/check_is_master.sql --connect --no-vacuum --progress 1 -T 5 --max-tries=1 &

wait -n || {
    cat /var/log/odyssey.log
    exit 1
}

wait -n || {
    cat /var/log/odyssey.log
    exit 1
}

ody-stop