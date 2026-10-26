#!/bin/bash -x

set -eux

source /tests/__helpers/watchdog.sh

watchdog_console_port=8432
watchdog_prepare

/usr/bin/odyssey /tests/lagpolling_port/lag-conf.conf
timeout 5 bash -c '
until pg_isready -h localhost -p 6432 -U user1 -d postgres; do
  echo "Wait for odyssey..."
  sleep 0.1
done
'

watchdog_pause
watchdog_set_lag 10

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

watchdog_resume
ody-stop
