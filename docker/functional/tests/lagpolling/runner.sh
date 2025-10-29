#!/bin/bash -x

set -eux

/usr/bin/odyssey /tests/lagpolling/lag-conf.conf
sleep 3

for _ in $(seq 1 3); do
	PGPASSWORD=lolol timeout 3s psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 2' && exit 1
done

sed -i 's/catchup_timeout 10/catchup_timeout 1000/g' /tests/lagpolling/lag-conf.conf

PGPASSWORD=lolol timeout 3s psql -h localhost -p6432 -dconsole -Uuser1 -c 'reload'
sleep 1

# for _ in $(seq 1 3); do
# 	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1 
# done

PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1
PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 4' || exit 1
PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 5' || exit 1

# service postgresql restart || true
# sudo -u postgres /usr/lib/postgresql/16/bin/pg_ctl -D /var/lib/postgresql/16/repl/ restart || true
# sudo -u postgres /usr/lib/postgresql/16/bin/pg_ctl -D /var/lib/postgresql/16/main/ restart || true

sleep 1

PGPASSWORD=lolol timeout 3s psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1

# test bad backend connections closed properly

for _ in $(seq 1 3); do
	PGPASSWORD=lolol timeout 3s psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1 
done

ody-stop
