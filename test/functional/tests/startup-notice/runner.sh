#!/bin/bash

set -ex

check_file() {
    local name=$1
    local user=$2

    cat /tests/startup-notice/sql/$name.sql | psql "host=localhost port=6432 user=$user dbname=broken_collation" --echo-all --no-psqlrc --quiet > /tests/startup-notice/results/$name.out 2>&1 || {
        cat /tests/startup-notice/results/$name.out
        exit 1
    }

    diff /tests/startup-notice/expected/$name.out /tests/startup-notice/results/$name.out || {
        cat /tests/startup-notice/results/$name.out
        xxd /tests/startup-notice/results/$name.out
        exit 1
    }
}

mkdir -p /tests/startup-notice/results/

/usr/bin/odyssey /tests/startup-notice/odyssey.conf
sleep 1

check_file '1' postgres

ody-stop
