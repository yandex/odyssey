#!/usr/bin/env bash

set -ex

/tests/ody_integration_test/ody_integration_test
sleep 5
ody-stop

odyssey /tests/ody_integration_test/pstmt-cache.conf
for attempt in {1..50}; do
    if pg_isready -h 127.0.0.1 -p 6432 -U postgres -d pstmt_single; then
        break
    fi
    sleep 0.1
done

/tests/ody_integration_test/ody_integration_test --pstmt-cache || {
    sleep 1
    cat /var/log/odyssey.log
    for i in /asan-output*; do
        cat $i || true
    done
    ody-stop
    exit 1
}

ody-stop
