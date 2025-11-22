#!/bin/bash -x

set -ex

mkdir /tests/rules_order/results/

/usr/bin/odyssey /tests/rules_order/seq_1.conf
sleep 1

timeout 1s psql 'host=localhost port=6432 user=vuser dbname=vdb' -c 'show rules;' --echo-all --quiet > /tests/rules_order/results/seq_1.out 2>&1 || {
    cat /tests/rules_order/results/seq_1.out
    exit 1
}

diff /tests/rules_order/seq_1.out /tests/rules_order/results/seq_1.out || {
    echo "start /tests/rules_order/seq_1.out"
    cat /tests/rules_order/seq_1.out
    echo "end /tests/rules_order/seq_1.out"

    echo

    echo "start /tests/rules_order/results/seq_1.out"
    cat /tests/rules_order/results/seq_1.out
    echo "end /tests/rules_order/results/seq_1.out"

    exit 1
}

rm /tests/rules_order/seq_1.conf
cp /tests/rules_order/seq_1_reload.conf /tests/rules_order/seq_1.conf

kill -sHUP $(pidof odyssey)
sleep 2

timeout 1s psql 'host=localhost port=6432 user=vuser dbname=vdb' -c 'show rules;' --echo-all --quiet > /tests/rules_order/results/seq_1_reload.out 2>&1 || {
    cat /tests/rules_order/results/seq_1_reload.out
    exit 1
}

diff /tests/rules_order/seq_1_reload.out /tests/rules_order/results/seq_1_reload.out || {
    echo "start /tests/rules_order/seq_1_reload.out"
    cat /tests/rules_order/seq_1_reload.out
    echo "end /tests/rules_order/seq_1_reload.out"

    echo

    echo "start /tests/rules_order/results/seq_1_reload.out"
    cat /tests/rules_order/results/seq_1_reload.out
    echo "end /tests/rules_order/results/seq_1_reload.out"

    exit 1
}

ody-stop

/usr/bin/odyssey /tests/rules_order/not_seq_1.conf
sleep 1

timeout 1s psql 'host=localhost port=6432 user=vuser dbname=vdb' -c 'show rules;' --echo-all --quiet > /tests/rules_order/results/not_seq_1.out 2>&1 || {
    cat /tests/rules_order/results/not_seq_1.out
    exit 1
}

diff /tests/rules_order/not_seq_1.out /tests/rules_order/results/not_seq_1.out || {
    echo "start /tests/rules_order/not_seq_1.out"
    cat /tests/rules_order/not_seq_1.out
    echo "end /tests/rules_order/not_seq_1.out"

    echo

    echo "start /tests/rules_order/results/not_seq_1.out"
    cat /tests/rules_order/results/not_seq_1.out
    echo "end /tests/rules_order/results/not_seq_1.out"

    exit 1
}

rm /tests/rules_order/not_seq_1.conf
cp /tests/rules_order/not_seq_1_reload.conf /tests/rules_order/not_seq_1.conf

kill -sHUP $(pidof odyssey)
sleep 2

timeout 1s psql 'host=localhost port=6432 user=vuser dbname=vdb' -c 'show rules;' --echo-all --quiet > /tests/rules_order/results/not_seq_1_reload.out 2>&1 || {
    cat /tests/rules_order/results/not_seq_1_reload.out
    exit 1
}

diff /tests/rules_order/not_seq_1_reload.out /tests/rules_order/results/not_seq_1_reload.out || {
    echo "start /tests/rules_order/not_seq_1_reload.out"
    cat /tests/rules_order/not_seq_1_reload.out
    echo "end /tests/rules_order/not_seq_1_reload.out"

    echo

    echo "start /tests/rules_order/results/not_seq_1_reload.out"
    cat /tests/rules_order/results/not_seq_1_reload.out
    echo "end /tests/rules_order/results/not_seq_1_reload.out"

    exit 1
}

ody-stop