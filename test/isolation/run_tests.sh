#!/bin/bash

# Runs the vanilla pg_isolation_regress suites against a running
# odyssey instance. Works around the only run-to-run output difference:
# isolationtester embeds the session backend PIDs into the description
# of its lock-wait probe statement, and odyssey assigns its own (fresh)
# backend keys on every connect, so those PIDs are masked in both the
# results and the expected output before comparing.
#
# Environment: PGHOST (default 127.0.0.1), PGPORT (default 6432),
# PGUSER (default postgres), PGDATABASE (default postgres),
# PG_ISOLATION_REGRESS (path to pg_isolation_regress).

set -u

PGHOST="${PGHOST:-127.0.0.1}"
PGPORT="${PGPORT:-6432}"
PGUSER="${PGUSER:-postgres}"
PGDATABASE="${PGDATABASE:-postgres}"
REGRESS_BIN="${PG_ISOLATION_REGRESS:-pg_isolation_regress}"

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/results"

mkdir -p "$OUT"

"$REGRESS_BIN" --inputdir="$DIR" --outputdir="$OUT" --use-existing \
	--dbname="$PGDATABASE" --host="$PGHOST" --port="$PGPORT" \
	--user="$PGUSER" --schedule="$DIR/schedule" >/dev/null 2>&1 || true

normalize() {
	# isolationtester embeds the session backend PIDs into the
	# description of its lock-wait probe statement, and odyssey assigns
	# its own (fresh) backend keys on every connect, so those PIDs are
	# masked - and the whole probe row is collapsed to a fixed form,
	# since its padding (and the pids) differ from run to run
	sed -E -e 's/\{[0-9]+(,[0-9]+)+\}/{PIDS}/g' \
		-e 's/^(odyssey_pstmt_[0-9]+)\|SELECT pg_catalog\.pg_isolation_test_session_is_blocked.*\|[[:space:]]*[0-9]+[[:space:]]*$/\1|ISOLATION_PROBE/' \
		"$1" > "$1.norm"
}

status=0
for expected in "$DIR/expected"/*.out; do
	name="$(basename "$expected" .out)"
	result="$OUT/results/$name.out"
	if [ ! -f "$result" ]; then
		echo "$name: no result output"
		status=1
		continue
	fi
	normalize "$expected"
	normalize "$result"
	if diff -u "$expected.norm" "$result.norm" > "$result.diffs"; then
		echo "$name: passed"
	else
		cat "$result.diffs"
		echo "$name: FAILED"
		status=1
	fi
done

exit "$status"
