#!/bin/bash

# Adapted from the isolation runner in PR #1706 (c16c06f5).
# isolationtester connects to PostgreSQL to observe real lock waits; the
# fixture connects to Odyssey and stops reading its virtual responses.
set -eu

export PGHOST="${PGHOST:-127.0.0.1}"
export PGPORT="${PGPORT:-5432}"
export PGUSER="${PGUSER:-postgres}"
export PGDATABASE="${PGDATABASE:-postgres}"
export PG_TEST_TIMEOUT_DEFAULT="${PG_TEST_TIMEOUT_DEFAULT:-5}"
REGRESS_BIN="${PG_ISOLATION_REGRESS:-pg_isolation_regress}"

DIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$DIR/results"
WORK="$(mktemp -d "$DIR/results/virtual-write-timeout.XXXXXX")"
mkdir -p "$WORK/input/specs" "$WORK/input/expected" "$WORK/output"
for spec in "$DIR"/specs/*.spec; do
    sed "s|@SIGNAL_DIR@|$WORK|g" "$spec" > "$WORK/input/specs/$(basename "$spec")"
done
for expected in "$DIR"/expected/*.out; do
    sed "s|@SIGNAL_DIR@|$WORK|g" "$expected" > "$WORK/input/expected/$(basename "$expected")"
done

python3 "$DIR/fixtures/virtual_write_timeout.py" "$WORK" > "$WORK/fixture.log" 2>&1 &
fixture_pid=$!
cleanup() {
    kill "$fixture_pid" 2>/dev/null || true
    wait "$fixture_pid" 2>/dev/null || true
}
trap cleanup EXIT

for ((i = 0; i < 200; ++i)); do
    if [ -f "$WORK/ready" ]; then
        break
    fi
    if ! kill -0 "$fixture_pid" 2>/dev/null; then
        cat "$WORK/fixture.log"
        exit 1
    fi
    sleep 0.05
done
if [ ! -f "$WORK/ready" ]; then
    cat "$WORK/fixture.log"
    echo "slow-client fixture did not become ready" >&2
    exit 1
fi

if ! "$REGRESS_BIN" --inputdir="$WORK/input" --outputdir="$WORK/output" \
    --use-existing --dbname="$PGDATABASE" --host="$PGHOST" --port="$PGPORT" \
    --user="$PGUSER" --schedule="$DIR/schedule"; then
    cat "$WORK/output/regression.diffs" 2>/dev/null || true
    cat "$WORK/fixture.log"
    exit 1
fi

# A process crash would also release the locks; require Odyssey to stay usable.
PGCONNECT_TIMEOUT=5 psql -X --no-password --set=ON_ERROR_STOP=1 \
    --host="${ODYSSEY_HOST:-127.0.0.1}" --port="${ODYSSEY_PORT:-6432}" \
    --username="${ODYSSEY_USER:-$PGUSER}" --dbname="$PGDATABASE" \
    --command='SELECT 1' >/dev/null
