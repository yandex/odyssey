#!/bin/bash -x

set -eux

reset_replica_lag() {
	psql 'host=localhost port=5433 user=postgres dbname=postgres' -c "ALTER SYSTEM RESET recovery_min_apply_delay;" || true
	psql 'host=localhost port=5433 user=postgres dbname=postgres' -c "SELECT pg_reload_conf();" || true
}

enable_replica_lag() {
	psql 'host=localhost port=5433 user=postgres dbname=postgres' -c "ALTER SYSTEM SET recovery_min_apply_delay = '15s'"
	psql 'host=localhost port=5433 user=postgres dbname=postgres' -c "SELECT pg_reload_conf();"

	trap reset_replica_lag EXIT
}

psql 'host=localhost port=5432 user=postgres dbname=postgres' -c 'DROP TABLE IF EXISTS wal_bump_data' 2>&1 || {
	exit 1
}

psql 'host=localhost port=5432 user=postgres dbname=postgres' -c 'CREATE TABLE IF NOT EXISTS wal_bump_data(num int)' 2>&1 || {
	exit 1
}

psql 'host=localhost port=5433 user=postgres dbname=postgres' -c 'SELECT TRUNC(EXTRACT(EPOCH FROM now() - pg_last_xact_replay_timestamp()))'

enable_replica_lag

psql 'host=localhost port=5432 user=postgres dbname=postgres' -c 'INSERT INTO wal_bump_data VALUES(42)' 2>&1 || {
	exit 1
}

psql 'host=localhost port=5432 user=postgres dbname=postgres' -c 'INSERT INTO wal_bump_data VALUES(43)' 2>&1 || {
	exit 1
}

/usr/bin/odyssey /tests/lagpolling_port/lag-conf.conf
timeout 5 bash -c '
until pg_isready -h localhost -p 6432 -U user1 -d postgres; do
  echo "Wait for odyssey..."
  sleep 0.1
done
'

sleep 6

psql 'host=localhost port=5433 user=postgres dbname=postgres' -c 'SELECT TRUNC(EXTRACT(EPOCH FROM now() - pg_last_xact_replay_timestamp()))'

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' && {
		cat /var/log/odyssey.log
		exit 1
	}
done

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p7432 -dpostgres -Uuser1 -c 'select 3' || {
		cat /var/log/odyssey.log
		exit 1
	}
done

ody-stop
