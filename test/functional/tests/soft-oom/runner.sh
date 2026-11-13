#!/bin/bash

set -e

load_soft_oom_force() {
    PGCONNECT_TIMEOUT=5 psql -X -v ON_ERROR_STOP=1 -h localhost -p 6432 -U console -d console -c 'LOAD soft_oom_force'
}

query_odyssey() {
    local output

    if output=$(PGCONNECT_TIMEOUT=5 psql -X -v ON_ERROR_STOP=1 -h localhost -p 6432 -U postgres -d postgres -c 'SELECT 1' 2>&1); then
        return 0
    fi

    printf 'new connection was not accepted, got:\n%s\n' "$output" >&2
    return 1
}

assert_soft_oom() {
    local output

    if output=$(PGCONNECT_TIMEOUT=5 psql -X -v ON_ERROR_STOP=1 -h localhost -p 6432 -U postgres -d postgres -c 'SELECT 1' 2>&1); then
        echo 'new connection was accepted during soft OOM' >&2
        return 1
    fi

    if [[ "$output" != *"soft out of memory"* ]]; then
        printf 'expected soft OOM, got:\n%s\n' "$output" >&2
        return 1
    fi

    return 0
}

ody-start /tests/soft-oom/conf.conf
query_odyssey

load_soft_oom_force
assert_soft_oom

load_soft_oom_force
query_odyssey