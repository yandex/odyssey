#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/npgsql_compat/config.conf

/tests/npgsql_compat/npgsql_compat.console || {
	echo "ERROR: npgsql-compat tests failed"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

ody-stop
