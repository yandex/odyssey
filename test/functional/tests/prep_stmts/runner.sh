#!/usr/bin/env bash

set -ex

/usr/bin/odyssey /tests/prep_stmts/pstmts.conf
sleep 1

/tests/prep_stmts/pstmts-test || {
    sleep 1
    cat /var/log/odyssey.log
    for i in /asan-output*; do
        cat $i || true
    done
    exit 1
}

ody-stop
