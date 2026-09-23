#!/bin/bash

set -euo pipefail

cache_tmp=$(mktemp -d)
cleanup() {
	rc=$?
	sql 'DELETE FROM auth_query_cache_test.gate' > /dev/null 2>&1 || true
	rm -rf "$cache_tmp"
	exit "$rc"
}
trap cleanup EXIT

fail() {
	echo "ERROR: $1"
	exit 1
}

sql() {
	psql -X -At -v ON_ERROR_STOP=1 -h localhost -p 5432 -U postgres \
		-d postgres -c "$1"
}

connect() {
	PGPASSWORD="$2" PGCONNECT_TIMEOUT=5 psql -X -At -v ON_ERROR_STOP=1 \
		-h "${3:-127.0.0.1}" -p 6432 -U "$1" \
		-d "${4:-auth_query_cache_db}" -c 'SELECT pg_backend_pid()'
}

reject() {
	if connect "$1" "$2" "${4:-127.0.0.1}" "${5:-auth_query_cache_db}" \
		> "$cache_tmp/rejected" 2>&1; then
		fail "accepted rejected credentials for $1"
	fi
	grep -q "$3" "$cache_tmp/rejected" ||
		fail "expected '$3' for $1, got: $(cat "$cache_tmp/rejected")"
}

alice_backend=$(connect cache_alice alpha)
bob_backend=$(connect cache_bob beta)
test "$alice_backend" = "$bob_backend" ||
	fail "cache_alice and cache_bob did not share the route backend"
reject cache_alice beta 'password authentication failed'
reject cache_bob alpha 'password authentication failed'
connect cache_alice alpha > /dev/null
connect cache_bob beta > /dev/null
test "$(sql 'SELECT count(*) FROM auth_query_cache_test.requests')" = 2 ||
	fail "repeated logins did not use cached passwords"

pids=()
for n in $(seq 1 8); do
	connect cache_parallel parallel > "$cache_tmp/parallel_$n" 2>&1 &
	pids+=("$!")
done
parallel_failed=0
for pid in "${pids[@]}"; do
	wait "$pid" || parallel_failed=1
done
test "$parallel_failed" = 0 ||
	fail "parallel first logins failed: $(cat "$cache_tmp"/parallel_*)"
test "$(sql "SELECT count(*) FROM auth_query_cache_test.requests WHERE username = 'cache_parallel'")" = 1 ||
	fail "parallel first logins ran more than one auth query"

reload_backend=$(connect cache_reload before 127.0.0.1 auth_query_reload_db)

sql "INSERT INTO auth_query_cache_test.failures VALUES ('cache_retry')"
reject cache_retry epsilon 'failed to make auth query'
sql "DELETE FROM auth_query_cache_test.failures WHERE username = 'cache_retry'"
connect cache_retry epsilon > /dev/null ||
	fail "login after a failed first auth query was not retried"

connect cache_error delta > /dev/null
sql "DELETE FROM auth_query_cache_test.credentials WHERE username = 'cache_alice';
     UPDATE auth_query_cache_test.credentials SET password = NULL WHERE username = 'cache_bob';
     INSERT INTO auth_query_cache_test.failures VALUES ('cache_error')"

deadline=$((SECONDS + 25))
alice_denied=0
bob_denied=0
while [ "$SECONDS" -lt "$deadline" ]; do
	if ! connect cache_alice alpha > "$cache_tmp/alice" 2>&1; then
		grep -q 'incorrect user' "$cache_tmp/alice" ||
			fail "unexpected cache_alice error: $(cat "$cache_tmp/alice")"
		alice_denied=1
	fi
	if ! connect cache_bob beta > "$cache_tmp/bob" 2>&1; then
		grep -q 'incorrect user' "$cache_tmp/bob" ||
			fail "unexpected cache_bob error: $(cat "$cache_tmp/bob")"
		bob_denied=1
	fi
	connect cache_error delta > /dev/null ||
		fail "cache_error lost its password after a failed refresh"
	if [ "$alice_denied" = 1 ] && [ "$bob_denied" = 1 ] &&
		grep -q 'auth query cache test failure for cache_error' /var/log/odyssey.log; then
		break
	fi
	sleep 0.1
done
test "$alice_denied" = 1 || fail "removed cache_alice was not denied"
test "$bob_denied" = 1 || fail "cache_bob with a NULL password was not denied"
grep -q 'auth query cache test failure for cache_error' /var/log/odyssey.log ||
	fail "cache_error refresh did not fail"
reject cache_alice alpha 'incorrect user'
reject cache_bob beta 'incorrect user'
connect cache_error delta > /dev/null ||
	fail "cache_error lost its password after a failed refresh"
reject cache_error wrong 'password authentication failed'

sql "INSERT INTO auth_query_cache_test.credentials
     SELECT 'cache_' || n, 'password_' || n FROM generate_series(1, 70) n"
for n in $(seq 1 70); do
	connect "cache_$n" "password_$n" > /dev/null
done
test "$(sql "SELECT count(*) FROM auth_query_cache_test.requests WHERE username = 'cache_1'")" = 1 ||
	fail "cache_1 was queried more than once while filling the cache"
connect cache_1 password_1 > /dev/null
test "$(sql "SELECT count(*) FROM auth_query_cache_test.requests WHERE username = 'cache_1'")" = 2 ||
	fail "cache_1 was not evicted from a full cache"

sql 'INSERT INTO auth_query_cache_test.gate VALUES (true)'
deadline=$((SECONDS + 20))
while [ "$(sql "SELECT count(*) FROM pg_stat_activity WHERE datname = 'postgres'
                AND query = 'SELECT * FROM auth_query_cache_test.lookup(\$1)'
                AND wait_event = 'PgSleep'")" = 0 ]; do
	connect cache_reload before 127.0.0.1 auth_query_reload_db > /dev/null
	test "$SECONDS" -lt "$deadline" ||
		fail "cache_reload refresh did not start"
	sleep 0.05
done

reloads=$(grep -c 'routes created/deleted and scheduled for removal' /var/log/odyssey.log || true)
sed -i '/database "auth_query_source" {/,/^}/ s/storage_db "postgres"/storage_db "auth_query_source_new"/' "$1"
kill -HUP "$(cat /var/run/odyssey.pid)"
deadline=$((SECONDS + 5))
while [ "$(grep -c 'routes created/deleted and scheduled for removal' /var/log/odyssey.log || true)" -le "$reloads" ]; do
	test "$SECONDS" -lt "$deadline" || fail "configuration was not reloaded"
	sleep 0.05
done
test "$(connect cache_reload after 127.0.0.1 auth_query_reload_db)" = "$reload_backend" ||
	fail "cache_reload did not use the reloaded source on its route"
reject cache_reload before 'password authentication failed' 127.0.0.1 auth_query_reload_db

sql 'DELETE FROM auth_query_cache_test.gate'
deadline=$((SECONDS + 5))
while [ "$(sql "SELECT count(*) FROM auth_query_cache_test.requests WHERE username = 'cache_reload'")" -lt 2 ]; do
	test "$SECONDS" -lt "$deadline" ||
		fail "the refresh started before reload did not finish"
	sleep 0.05
done
test "$(connect cache_reload after 127.0.0.1 auth_query_reload_db)" = "$reload_backend" ||
	fail "the refresh started before reload replaced the new password"
reject cache_reload before 'password authentication failed' 127.0.0.1 auth_query_reload_db
