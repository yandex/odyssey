#!/usr/bin/env bash

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

/odyssey/build/sources/odyssey /odyssey/docker/prometheus-legacy/config.conf
sleep 3

netstat -tulpan | grep ":7777" || {
  echo "Can't find prom port"
  ody-stop
  exit 1
}
netstat -tulpan | grep ":6432" || {
  echo "Can't find odyssey port"
  ody-stop
  exit 1
}

for (( i=1; i <= 10; i++ ))
do 
psql "host=localhost port=6432 dbname=postgres user=postgres" -A -t -c "select $i" || {
    echo "error: failed to successfully auth"
    cat /var/log/odyssey.log
    ody-stop
    exit 1
}
done

curl  -v http://localhost:7777/metrics | grep "TYPE" || {
  echo "Can't find odyssey metrics in prometheus format"
  ody-stop
  exit 1
}
sleep 1

sleep 1

curl  "http://prometheus:9090/api/v1/targets/metadata?job=odyssey" | jq | grep "odyssey" || {
  echo "Can't find odyssey metrics from prom"
  ody-stop
  exit 1
}


ody-stop

if ps aux | grep -q '[o]dyssey'; then
  echo "Can't finish odyssey after sigterm"
fi