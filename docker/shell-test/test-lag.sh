#!/bin/bash -x


/usr/bin/odyssey /etc/odyssey/lag-conf.conf

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' && exit 1 
done

sed -i 's/catchup_timeout 10/catchup_timeout 1000/g' /etc/odyssey/lag-conf.conf

PGPASSWORD=lolol psql -h localhost -p6432 -dconsole -Uuser1 -c 'reload'

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1 
done
