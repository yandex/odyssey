#!/bin/bash -x

set -eux

source /tests/__helpers/watchdog.sh

watchdog_prepare

/usr/bin/odyssey /tests/lagpolling/lag-conf.conf
timeout 5 bash -c '
until pg_isready -h localhost -p 6432 -U user1 -d postgres; do
  echo "Wait for odyssey..."
  sleep 0.1
done
' || {
	echo "Failed to start odyssey"
	sleep 1
	cat /var/log/odyssey.log
	exit 1
}

watchdog_pause

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || {
		sleep 1
		cat /var/log/odyssey.log
		exit 1
	}
done

watchdog_set_lag 10

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' && exit 1
done

sed -i 's/catchup_timeout 5/catchup_timeout 20/g' /tests/lagpolling/lag-conf.conf

PGPASSWORD=lolol psql -h localhost -p6432 -dconsole -Uuser1 -c 'reload'

for _ in $(seq 1 3); do
	PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || exit 1
done

watchdog_resume
ody-stop
