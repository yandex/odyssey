#!/bin/bash -x

# Same scenario as lagpolling, but the watchdog connects to broken_collation,
# where PostgreSQL emits a WARNING during every connection startup.
# A startup notice must not break internal clients: lag polling must keep
# working and catchup_timeout must still reject clients on a lagging replica.

set -eux

source /tests/__helpers/watchdog.sh

psql 'host=localhost port=5433 user=postgres dbname=broken_collation' -c 'select 1' 2>&1 | grep -q 'WARNING' || {
	echo 'precondition failed: broken_collation must emit a startup WARNING on the replica'
	exit 1
}

watchdog_prepare broken_collation

/usr/bin/odyssey /tests/lagpolling_startup_notice/lag-conf.conf
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

odyssey_pid=$(cat /var/run/odyssey.pid)

watchdog_pause

for _ in $(seq 1 3); do
	psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' || {
		sleep 1
		cat /var/log/odyssey.log
		exit 1
	}
done

watchdog_set_lag 10

# the replica lags more than catchup_timeout, so clients must be rejected
for _ in $(seq 1 3); do
	if out=$(psql -h localhost -p6432 -dpostgres -Uuser1 -c 'select 3' 2>&1); then
		echo 'client was not rejected: watchdog did not update replication lag'
		cat /var/log/odyssey.log
		exit 1
	fi

	echo "$out" | grep -q 'replication lag' || {
		echo "client was rejected for another reason: $out"
		cat /var/log/odyssey.log
		exit 1
	}
done

kill -0 "$odyssey_pid" || {
	echo 'odyssey is not running'
	cat /var/log/odyssey.log
	exit 1
}

if grep -q 'attach failed with status' /var/log/odyssey.log; then
	echo 'watchdog failed to attach'
	cat /var/log/odyssey.log
	exit 1
fi

watchdog_resume
ody-stop
