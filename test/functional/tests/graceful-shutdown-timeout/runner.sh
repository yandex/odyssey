#!/bin/bash -x

set -exu

/usr/bin/odyssey /tests/graceful-shutdown-timeout/config.conf &
odyssey_pid=$!
sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select 1'

psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select pg_sleep(10)' &
query_pid=$!
sleep 1

start_time=$(date +%s)

kill -s TERM $odyssey_pid

wait $odyssey_pid || true

end_time=$(date +%s)
elapsed=$((end_time - start_time))

if [ $elapsed -lt 5 ]; then
    echo "Shutdown timeout was configured to 5 seconds"
	exit 1
fi
