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

start_waiting_connection() {
    local i

    client_log=$(mktemp)
    trap 'kill "$client_pid" 2>/dev/null || true; rm -f -- "$client_log"' EXIT

    LC_ALL=C PGCONNECT_TIMEOUT=5 timeout 20s \
        psql -X -v ON_ERROR_STOP=1 -v VERBOSITY=verbose \
        -h localhost -p 6432 -U postgres -d postgres \
        >"$client_log" 2>&1 <<'SQL' &
SELECT 1;
\echo client_ready
\watch 0.25
SQL
    client_pid=$!

    for ((i = 0; i < 50; i++)); do
        if grep -qx 'client_ready' "$client_log"; then
            return 0
        fi
        sleep 0.1
    done

    echo 'existing connection did not become ready' >&2
    cat "$client_log" >&2
    return 1
}

assert_connection_dropped() {
    local rc=0

    wait "$client_pid" || rc=$?
    client_pid=

    if [[ "$rc" == 124 ]] || ! grep -q 'FATAL:.*57P01' "$client_log"; then
        echo 'expected existing connection to be dropped' >&2
        cat "$client_log" >&2
        return 1
    fi
}

ody-start /tests/soft-oom/conf.conf
query_odyssey
start_waiting_connection

load_soft_oom_force
assert_soft_oom
assert_connection_dropped

load_soft_oom_force
query_odyssey