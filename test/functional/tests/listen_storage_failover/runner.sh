#!/bin/bash

set -ex

/usr/bin/odyssey /tests/listen_storage_failover/odyssey.conf

timeout 5 bash -c '
until pg_isready -h localhost -p 6432 -U postgres -d postgres; do
  echo "Wait for odyssey on 6432..."
  sleep 0.1
done
'

timeout 5 bash -c '
until pg_isready -h localhost -p 7432 -U postgres -d postgres; do
  echo "Wait for odyssey on 7432..."
  sleep 0.1
done
'

timeout 5 bash -c '
until pg_isready -h localhost -p 8432 -U postgres -d postgres; do
  echo "Wait for odyssey on 8432..."
  sleep 0.1
done
'

psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'SELECT 1' || {
    cat /var/log/odyssey.log
    echo "FAIL: port 6432 should succeed via fallback to live_storage"
    exit 1
}

psql 'host=localhost port=7432 user=postgres dbname=postgres' -c 'SELECT 1' || {
    cat /var/log/odyssey.log
    echo "FAIL: port 7432 should succeed connecting to live_storage directly"
    exit 1
}

psql 'host=localhost port=8432 user=postgres dbname=postgres' -c 'SELECT 1' 2>/dev/null && {
    cat /var/log/odyssey.log
    echo "FAIL: port 8432 should fail when all storages are dead"
    exit 1
}

for i in $(seq 1 5); do
    psql 'host=localhost port=6432 user=postgres dbname=postgres' -c "SELECT $i" || {
        cat /var/log/odyssey.log
        echo "FAIL: repeated connection $i through failover port should work"
        exit 1
    }
done

ody-stop
