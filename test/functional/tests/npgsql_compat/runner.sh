#!/bin/bash -x

set -ex

ody-start2 /tests/npgsql_compat/config.conf auth_query_db auth_query_user_scram_sha_256

/tests/npgsql_compat/NpgsqlOdysseyScram.Console || {
	echo "ERROR: npgsql-compat tests failed"

	for i in /asan-output*; do
		cat $i || true
	done

	sleep 1

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

ody-stop || {
	for i in /asan-output*; do
		cat $i || true
	done

	sleep 1

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

exit 0
