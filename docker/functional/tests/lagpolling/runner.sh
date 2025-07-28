#!/bin/bash -x


# disable options for this run
# because it has some bug that leads to odyssey connection stuck
# if options is defined
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

# sudo -u postgres /usr/lib/postgresql/16/bin/pg_ctl -D /var/lib/postgresql/16/repl/ restart
# until pg_isready -h localhost -p 5433 -U postgres -d postgres; do
#   echo "Wait for replica..."
#   sleep 1
# done

# sudo -u postgres /usr/lib/postgresql/16/bin/pg_ctl -D /var/lib/postgresql/16/main/ restart
# until pg_isready -h localhost -p 5432 -U postgres -d postgres; do
#   echo "Wait for primary..."
#   sleep 1
# done

# PGPASSWORD=lolol timeout 1s psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1

# # test bad backend connections closed properly

# for _ in $(seq 1 3); do
# 	PGPASSWORD=lolol timeout 1s psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1 
# done

ody-stop
