#!/bin/bash

set -ex

check_file() {
    local name=$1
    local user=$2

    cat /tests/broken_cached_plan/sql/$name.sql | psql "host=localhost port=6432 user=$user dbname=postgres" --echo-all --no-psqlrc --quiet > /tests/broken_cached_plan/results/$name.out 2>&1 || {
        cat /tests/broken_cached_plan/results/$name.out
        exit 1
    }

    diff /tests/broken_cached_plan/expected/$name.out /tests/broken_cached_plan/results/$name.out || {
        cat /tests/broken_cached_plan/results/$name.out
        xxd /tests/broken_cached_plan/results/$name.out
        exit 1
    }
}

mkdir -p /tests/broken_cached_plan/results/

/usr/bin/odyssey /tests/broken_cached_plan/odyssey.conf
sleep 1

check_file '1' postgres

ody-stop
