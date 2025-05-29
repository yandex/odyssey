#!/bin/bash -x


/usr/bin/odyssey /tests/lagpolling/lag-conf.conf
sleep 3

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' && exit 1 
done

sed -i 's/catchup_timeout 10/catchup_timeout 1000/g' /tests/lagpolling/lag-conf.conf

PGPASSWORD=lolol psql -h localhost -p6432 -dconsole -Uuser1 -c 'reload'

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1 
done

service postgresql restart || true

sleep 1

PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1

# test bad backend connections closed properly

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1 
done

ody-stop
