#!/bin/bash

set -ex

/usr/bin/odyssey /tests/sql_guard_whitelist/odyssey.conf
sleep 1

CONNSTR='host=localhost port=6432 user=postgres dbname=postgres'
PASS=0
FAIL=0

check_blocked() {
	local desc="$1"
	local query="$2"
	local out
	out=$(psql "$CONNSTR" -c "$query" 2>&1) || true
	if echo "$out" | grep -q "query blocked by security policy"; then
		echo "PASS (blocked): $desc"
		PASS=$((PASS + 1))
	else
		echo "FAIL (not blocked): $desc"
		echo "  query: $query"
		echo "  output: $out"
		FAIL=$((FAIL + 1))
	fi
}

check_allowed() {
	local desc="$1"
	local query="$2"
	local expect="$3"
	local out
	out=$(psql "$CONNSTR" -c "$query" 2>&1)
	if echo "$out" | grep -q "$expect"; then
		echo "PASS (allowed): $desc"
		PASS=$((PASS + 1))
	else
		echo "FAIL (unexpectedly blocked or wrong output): $desc"
		echo "  query: $query"
		echo "  output: $out"
		FAIL=$((FAIL + 1))
	fi
}

# check_not_blocked verifies query is NOT blocked by sql_guard
# (may still fail at PG level, which is fine)
check_not_blocked() {
	local desc="$1"
	local query="$2"
	local out
	out=$(psql "$CONNSTR" -c "$query" 2>&1) || true
	if echo "$out" | grep -q "query blocked by security policy"; then
		echo "FAIL (unexpectedly blocked): $desc"
		echo "  query: $query"
		echo "  output: $out"
		FAIL=$((FAIL + 1))
	else
		echo "PASS (not blocked): $desc"
		PASS=$((PASS + 1))
	fi
}

# === Queries that SHOULD PASS (match the whitelist regex) ===

check_allowed "simple SELECT" \
	"SELECT 1 AS result;" "result"

check_allowed "SELECT with WHERE" \
	"SELECT 'hello' AS greeting WHERE 1=1;" "hello"

check_allowed "SELECT count" \
	"SELECT count(*) FROM pg_class WHERE relkind='r';" "count"

check_allowed "BEGIN transaction" \
	"BEGIN;" "BEGIN"

check_allowed "COMMIT transaction" \
	"COMMIT;" "COMMIT"

check_allowed "ROLLBACK" \
	"ROLLBACK;" "ROLLBACK"

check_allowed "SET statement" \
	"SET statement_timeout = '5s';" "SET"

check_allowed "SHOW statement" \
	"SHOW server_version;" "server_version"

# INSERT/UPDATE/DELETE match the whitelist regex but may fail at PG level
# (no target table) — just verify they are not blocked by sql_guard
check_not_blocked "INSERT INTO (not blocked by guard)" \
	"INSERT INTO nonexistent_table(id) VALUES (1);"

check_not_blocked "UPDATE (not blocked by guard)" \
	"UPDATE nonexistent_table SET id = 2 WHERE id = 1;"

check_not_blocked "DELETE FROM (not blocked by guard)" \
	"DELETE FROM nonexistent_table WHERE id = 2;"

# === Queries that SHOULD BE BLOCKED (don't match whitelist regex) ===

check_blocked "DROP TABLE (not whitelisted)" \
	"DROP TABLE IF EXISTS test_table;"

check_blocked "TRUNCATE (not whitelisted)" \
	"TRUNCATE TABLE pg_class;"

check_blocked "CREATE TABLE (not whitelisted)" \
	"CREATE TABLE evil(id int);"

check_blocked "ALTER TABLE (not whitelisted)" \
	"ALTER TABLE pg_class ADD COLUMN name text;"

check_blocked "COPY FROM PROGRAM (not whitelisted)" \
	"COPY shell FROM PROGRAM 'cat /etc/passwd';"

check_blocked "CREATE FUNCTION (not whitelisted)" \
	"CREATE OR REPLACE FUNCTION evil() RETURNS void AS 'BEGIN END' LANGUAGE plpgsql;"

check_blocked "GRANT (not whitelisted)" \
	"GRANT ALL ON ALL TABLES IN SCHEMA public TO postgres;"

check_blocked "REVOKE (not whitelisted)" \
	"REVOKE ALL ON ALL TABLES IN SCHEMA public FROM postgres;"

check_blocked "VACUUM (not whitelisted)" \
	"VACUUM FULL;"

check_blocked "REINDEX (not whitelisted)" \
	"REINDEX TABLE pg_class;"

check_blocked "DO block (not whitelisted)" \
	"DO \$\$ BEGIN RAISE NOTICE 'test'; END \$\$;"

# === Case-insensitive: whitelisted queries in different cases ===

check_allowed "select lowercase" \
	"select 1 as val;" "val"

check_allowed "Select mixed case" \
	"Select 'ok' as msg;" "ok"

# === Verify odyssey log contains blocked entries ===
grep -q "query blocked by sql_guard_regex" /var/log/odyssey.log || {
	echo "FAIL: no blocked queries found in odyssey log"
	FAIL=$((FAIL + 1))
}

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ "$FAIL" -gt 0 ]; then
	exit 1
fi

ody-stop
