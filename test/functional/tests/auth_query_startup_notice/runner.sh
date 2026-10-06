#!/bin/bash -x

# auth_query connects to broken_collation, where PostgreSQL emits a WARNING
# during every connection startup. A startup notice must not break the
# internal auth_query client, so the login must still succeed.

set -eux

psql 'host=127.0.0.1 port=5432 user=postgres dbname=broken_collation' -c 'select 1' 2>&1 | grep -q 'WARNING' || {
	echo 'precondition failed: broken_collation must emit a startup WARNING'
	exit 1
}

/usr/bin/odyssey /tests/auth_query_startup_notice/config.conf
sleep 1

odyssey_pid=$(cat /var/run/odyssey.pid)

PGPASSWORD=passwd PGCONNECT_TIMEOUT=5 psql -h 127.0.0.1 -p 6432 \
	-U auth_query_user_scram_sha_256 -c 'SELECT 1' auth_query_db || {
	echo 'login through auth_query failed'
	sleep 1
	cat /var/log/odyssey.log
	exit 1
}

kill -0 "$odyssey_pid" || {
	echo 'odyssey is not running'
	sleep 1
	cat /var/log/odyssey.log
	exit 1
}

ody-stop
