#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/auth_query/config.conf
sleep 1

timeout 20s pgbench 'host=localhost port=6432 user=auth_query_user_scram_sha_256 dbname=auth_query_db password=passwd' -f /tests/auth_query/select.sql -T 10 --connect --no-vacuum -j2 -c2 --progress 1 || {
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
timeout 20s pgbench 'host=localhost port=6432 user=auth_query_user_md5 dbname=auth_query_db password=passwd' -f /tests/auth_query/select.sql -T 10 --connect --no-vacuum -j2 -c2 --progress 1 || {
	echo "ERROR: failed backend auth with correct password"
	sleep 1

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

ody-stop
