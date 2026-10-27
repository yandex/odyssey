#!/usr/bin/env bash

set -eu

status_file="${PGTEST_DIR}/results/replay.status"
log_file="${PGTEST_DIR}/results/replay.log"
export PGCONNECT_TIMEOUT=5
export PGOPTIONS='-c statement_timeout=5000'
export PGAPPNAME=rwr_observer

replica_sql() {
    psql -X -qAt -v ON_ERROR_STOP=1 -h 127.0.0.1 -p 5434 \
        -U postgres -d postgres -c "$1"
}

wait_for() {
    local query=$1
    local result
    for ((i = 0; i < 200; ++i)); do
        result=$(replica_sql "$query")
        if [ "$result" = t ]; then
            return 0
        fi
        sleep 0.05
    done
    echo "timed out waiting for: $query" >&2
    return 1
}

finish() {
    local rc=$?
    if ((rc != 0)); then
        replica_sql 'SELECT pg_wal_replay_resume()' >/dev/null 2>&1 || true
    fi
    if [ "$1" = observe ]; then
        printf '%s\n' "$rc" > "$status_file"
    fi
}
trap 'finish "$1"' EXIT
trap 'exit 1' HUP INT TERM

case "$1" in
    pause)
        : > "$status_file"
        wait_for "SELECT to_regclass('public.rwr_test') IS NOT NULL"
        replica_sql 'SELECT pg_wal_replay_pause()' >/dev/null
        wait_for "SELECT pg_get_wal_replay_pause_state() = 'paused'"
        echo 'replica replay is paused'
        ;;
    observe)
        {
            wait_for "SELECT pg_get_wal_replay_pause_state() = 'paused'
                AND EXISTS (SELECT FROM pg_stat_activity
                    WHERE application_name = 'rwr_5node'
                    AND state = 'active' AND query LIKE 'WAIT FOR LSN %'
                    AND wait_event = 'WaitForWalReplay')"
            replica_sql 'SELECT pg_wal_replay_resume()' >/dev/null
        } > "$log_file" 2>&1
        ;;
    check)
        for ((i = 0; i < 400; ++i)); do
            if [ -s "$status_file" ]; then
                break
            fi
            sleep 0.05
        done
        if [ ! -s "$status_file" ] || [ "$(cat "$status_file")" != 0 ]; then
            cat "$log_file" >&2
            echo 'did not observe Odyssey blocked on WAIT FOR LSN' >&2
            exit 1
        fi
        echo 'observed Odyssey blocked on WAIT FOR LSN before resuming replay'
        ;;
    *)
        echo "usage: $0 pause|observe|check" >&2
        exit 1
        ;;
esac
