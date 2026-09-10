#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/auth_query/config.conf
sleep 1

odyssey_pid=$(cat /var/run/odyssey.pid)

check_null_password_rejected() {
	if PGPASSWORD=passwd PGCONNECT_TIMEOUT=5 psql -h localhost -p 6432 \
		-U auth_query_user_null_password -c "SELECT 1" auth_query_db; then
		echo "ERROR: auth_query accepted a user with a NULL password"
		cat /var/log/odyssey.log
		exit 1
	fi

	if ! kill -0 "$odyssey_pid"; then
		echo "ERROR: Odyssey crashed after auth_query returned a NULL password"
		for i in /asan-output*; do
			if [ -f "$i" ]; then
				cat "$i"
			fi
		done
		cat /var/log/odyssey.log
		cat /var/log/postgresql/postgresql-16-main.log
		exit 1
	fi
}

# The first attempt populates the negative cache entry; the second reuses it.
check_null_password_rejected
check_null_password_rejected

timeout 25s pgbench 'host=localhost port=6432 user=auth_query_user_scram_sha_256 dbname=auth_query_db password=passwd' -f /tests/auth_query/select.sql -T 21 --connect --no-vacuum -j2 -c2 --progress 1 || {
	echo "ERROR: failed backend auth with correct password"
	sleep 1

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

#PGPASSWORD=passwd psql -h localhost -p 6432 -U auth_query_user_scram_sha_256 -c "SELECT 1" auth_query_db >/dev/null 2>&1 || {
#PGPASSWORD=passwd psql -h localhost -p 6432 -U auth_query_user_md5 -c "SELECT 1" auth_query_db >/dev/null 2>&1 || {
timeout 25s pgbench 'host=localhost port=6432 user=auth_query_user_md5 dbname=auth_query_db password=passwd' -f /tests/auth_query/select.sql -T 21 --connect --no-vacuum -j2 -c2 --progress 1 || {
	echo "ERROR: failed backend auth with correct password"
	sleep 1

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

ody-stop
