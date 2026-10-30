#!/usr/bin/env bash

# Adapted from the native isolation runner in PR #1706 (c16c06f5).
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="${ISOLATION_OUTPUTDIR:-$DIR/output}"
REGRESS_BIN="${PG_ISOLATION_REGRESS:-pg_isolation_regress}"

mkdir -p "$OUT"

exec "$REGRESS_BIN" --inputdir="$DIR" --outputdir="$OUT" --use-existing \
    --dbname="${PGDATABASE:-postgres}" --host="${PGHOST:-127.0.0.1}" \
    --port="${PGPORT:-6432}" --user="${PGUSER:-postgres}" \
    --schedule="$DIR/schedule" "$@"
