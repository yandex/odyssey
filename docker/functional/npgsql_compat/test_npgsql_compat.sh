#!/bin/bash -x

set -ex

/usr/bin/odyssey /npgsql_compat/config.conf

/npgsql_compat/src/NpgsqlOdysseyScram.Console/bin/Debug/net9.0/NpgsqlOdysseyScram.Console || {
	echo "ERROR: npgsql-compat tests failed"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

ody-stop
