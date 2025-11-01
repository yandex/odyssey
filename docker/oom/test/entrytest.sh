#!/bin/bash

set -e

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done


psql 'host=odyssey port=6432 user=postgres dbname=postgres' -c 'select 1' || {
    echo "error: failed to make query"
    ody-stop
    exit 1
}


pgbench -i 'host=odyssey port=6432 user=postgres dbname=postgres'

START_CLIENTS=5
MAX_CLIENTS=128
STEP=5
DURATION=10

echo "select repeat('a',1024*1024*50)" > /tmp/load.txt


for ((c=START_CLIENTS; c<=MAX_CLIENTS; c+=STEP))
do
  output=$(pgbench 'host=odyssey port=6432 user=postgres dbname=postgres' -c $c -j $c -t $DURATION -f /tmp/load.txt -C 2>&1 || true)
  if echo "$output" | grep -q "soft out of memory" ; then
    echo "OK: Soft oom found!"
    exit 0
  fi
done

echo "ERROR: Soft oom not found"
exit 1
