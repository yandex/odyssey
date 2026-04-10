#!/bin/bash

set -ex

check_file() {
    local name=$1
    local user=$2

    cat /tests/idle_timeout/sql/$name.sql | psql "host=localhost port=6432 user=$user dbname=postgres" --echo-all --no-psqlrc --quiet > /tests/idle_timeout/results/$name.out 2>&1 && {
        # ignore error - the connection reset is expected
        # exact error will be cheched at diff step anyway
        cat /tests/idle_timeout/results/$name.out
        exit 1
    }

    # replace CID with XXX, it changes from run to run anyway
    sed -E -i 's/c[0-9a-f]{12}/cXXXXXXXXXXXX/g' /tests/idle_timeout/results/$name.out

    diff /tests/idle_timeout/expected/$name.out /tests/idle_timeout/results/$name.out || {
        cat /tests/idle_timeout/results/$name.out
        xxd /tests/idle_timeout/results/$name.out
        exit 1
    }
}

mkdir -p /tests/idle_timeout/results/

/usr/bin/odyssey /tests/idle_timeout/odyssey.conf
sleep 1

check_file tx tuser

check_file session suser

ody-stop
