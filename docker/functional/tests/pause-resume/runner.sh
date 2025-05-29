#!/bin/bash

set -uex

check_session() {
    /usr/bin/odyssey /tests/pause-resume/session.conf
    sleep 1

    psql -h localhost -p 6432 -c 'select pg_sleep(5)' -U postgres -d postgres 2>&1 &
    sleep 1
    psql -h localhost -p 6432 -c 'pause' -U console -d console
    sleep 1
    wait -n || {
        echo "long query must complete even if pause enabled"
        exit 1
    }

    if psql -h localhost -p 6432 -c 'select 1' -U postgres -d postgres; then
        echo "new queries must fail until resume executed"
        exit 1
    fi
    psql -h localhost -p 6432 -c 'resume' -U console -d console
    sleep 1
    psql -h localhost -p 6432 -c 'select 1' -U postgres -d postgres || {
        echo "after resume queries must work correctly"
        exit 1
    }

    ody-stop
}

check_transaction() {
    /usr/bin/odyssey /tests/pause-resume/transaction.conf
    sleep 1

    pgbench 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 15 &
    sleep 1

    psql -h localhost -p 6432 -c 'pause' -U console -d console
    sleep 6

    timeout 1s psql -h localhost -p 6432 -c 'select 1' -U postgres -d postgres
    if [ $? != 124 ]; then
        echo "new queries must fail with timeout until resume executed"
        exit 1
    fi
    psql -h localhost -p 6432 -c 'resume' -U console -d console

    wait -n || {
        echo "continious load on transaction should not fall"
        exit 1
    }

    psql -h localhost -p 6432 -c 'select 1' -U postgres -d postgres || {
        echo "after resume queries must work correctly"
        exit 1
    }

    ody-stop
}

check_session || exit 1
check_transaction || exit 1
