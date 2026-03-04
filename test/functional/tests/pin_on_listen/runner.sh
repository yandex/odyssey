#!/bin/bash

set -ex

/usr/bin/odyssey /tests/pin_on_listen/odyssey.conf
sleep 1

mkdir -p /tests/pin_on_listen/results/

cat /tests/pin_on_listen/sql/1.sql | psql 'host=localhost port=6432 user=postgres dbname=postgres' --echo-all --quiet >/tests/pin_on_listen/results/1.out 2>&1 &
listen_pid=$!

sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres' -c "notify example, 'Have you seen my coffee cup?'" || exit 1

wait $listen_pid || {
    cat /tests/pin_on_listen/results/1.out
    exit 1
}

# replace PID with XXX, it changes from run to run anyway
sed -E -i 's/PID [0-9]+/PID XXX/g' /tests/pin_on_listen/results/1.out

diff /tests/pin_on_listen/expected/1.out /tests/pin_on_listen/results/1.out || {
    cat /tests/pin_on_listen/results/1.out
    xxd /tests/pin_on_listen/results/1.out
    exit 1
}

ody-stop
