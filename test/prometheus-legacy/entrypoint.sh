#!/usr/bin/env bash

set -euxo pipefail

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

trap 'ody-stop || exit 1' EXIT
ODYSSEY_BIN=/odyssey/build/sources/odyssey ody-start /odyssey/test/prometheus-legacy/config.conf

netstat -tulpan | grep ":7777" || {
  echo "Can't find prom port"
  exit 1
}
netstat -tulpan | grep ":6432" || {
  echo "Can't find odyssey port"
  exit 1
}

for (( i=1; i <= 10; i++ ))
do 
psql "host=localhost port=6432 dbname=postgres user=postgres" -A -t -c "select $i" || {
    echo "error: failed to successfully auth"
    cat /var/log/odyssey.log
    exit 1
}
done

curl -fsS --max-time 2 http://localhost:7777/metrics | grep "TYPE" || {
  echo "Can't find odyssey metrics in prometheus format"
  exit 1
}
# Wait for Odyssey's stats update and a subsequent Prometheus scrape.
deadline=$((SECONDS + 30))
while true; do
  if response=$(curl -fsSG --max-time 2 \
      --data-urlencode 'query=sum(clients_processed{job="odyssey"})' \
      http://prometheus:9090/api/v1/query) &&
      jq -e '.status == "success" and .data.resultType == "vector" and
        any(.data.result[]; (.value[1] | tonumber) >= 10)' <<< "$response" >/dev/null; then
    break
  fi

  if (( SECONDS >= deadline )); then
    echo "Prometheus did not observe the 10 processed clients within 30 seconds" >&2
    printf '%s\n' "$response" >&2
    curl -fsS --max-time 2 http://prometheus:9090/api/v1/targets >&2 || true
    exit 1
  fi
  sleep 1
done


ody-stop

if ps aux | grep -q '[o]dyssey'; then
  echo "Can't finish odyssey after sigterm"
fi
