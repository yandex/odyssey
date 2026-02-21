#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/shared_pool/odyssey.conf
sleep 1

psql 'host=localhost port=6432 user=user1 dbname=db1' -c 'select pg_sleep(5)' &
first_pid=$!
sleep 0.5

psql 'host=localhost port=6432 user=user1 dbname=postgres' -c 'select pg_sleep(5)' &
second_pid=$!
sleep 0.5

psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'

set +e
timeout 1s psql 'host=localhost port=6432 user=user1 dbname=db1' -c 'select 42'
ret=$?
set -e
if [ $ret -ne 124 ]; then
    echo "Expected timeout (124), got $ret"
    cat /var/log/odyssey.log
    exit 1
fi

set +e
timeout 1s psql 'host=localhost port=6432 user=user1 dbname=postgres' -c 'select 42'
ret=$?
set -e
if [ $ret -ne 124 ]; then
    echo "Expected timeout (124), got $ret"
    cat /var/log/odyssey.log
    exit 1
fi

wait $first_pid || {
    cat /var/log/odyssey.log
    exit 1
}

wait $second || {
    cat /var/log/odyssey.log
    exit 1
}


ody-stop
