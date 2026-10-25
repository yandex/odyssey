#!/bin/bash

set -euo pipefail

results=$(mktemp -d)
cleanup() {
    local status=$?
    ody-stop || status=1
    rm -rf "$results"
    exit "$status"
}
trap cleanup EXIT

/usr/bin/odyssey /tests/session-params/odyssey.conf
timeout 5 bash -c 'until pg_isready -h 127.0.0.1 -p 6432 -U session_params -d postgres >/dev/null; do sleep 0.05; done'

sql=/tests/session-params/sql/reset.sql
previous_app=
previous_pid=
for app in first_client first_client second_client; do
    PGAPPNAME="$app" psql -X -Atq -v ON_ERROR_STOP=1 \
        'host=127.0.0.1 port=5432 user=postgres dbname=postgres connect_timeout=5' \
        -f "$sql" > "$results/direct.out"
    PGAPPNAME="$app" psql -X -Atq -v ON_ERROR_STOP=1 \
        'host=127.0.0.1 port=6432 user=session_params dbname=postgres connect_timeout=5' \
        -f "$sql" > "$results/odyssey.out"

    # The last row is the backend PID, checked separately for connection reuse.
    diff -u <(sed '$d' "$results/direct.out") <(sed '$d' "$results/odyssey.out")
    pid=$(tail -n 1 "$results/odyssey.out")
    if [ "$app" = "$previous_app" ] && [ "$pid" != "$previous_pid" ]; then
        echo "Backend was not reused with application_name=$app"
        exit 1
    fi
    previous_app=$app
    previous_pid=$pid
done

echo "session application_name reset: ok"
