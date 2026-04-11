#!/bin/bash

set -ex

check_file() {
    local name=$1
    local user=$2

    cat /tests/broken_conn/sql/$name.sql | psql "host=localhost port=6432 user=$user dbname=postgres" --echo-all --no-psqlrc --quiet > /tests/broken_conn/results/$name.out 2>&1 || {
        cat /tests/broken_conn/results/$name.out
        exit 1
    }

    diff /tests/broken_conn/expected/$name.out /tests/broken_conn/results/$name.out || {
        cat /tests/broken_conn/results/$name.out
        xxd /tests/broken_conn/results/$name.out
        exit 1
    }
}

mkdir -p /tests/broken_conn/results/

/usr/bin/odyssey /tests/broken_conn/odyssey.conf
sleep 1

check_file '1' postgres

ody-stop
