#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/npgsql_compat/config.conf

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
