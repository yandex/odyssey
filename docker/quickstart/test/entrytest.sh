#!/bin/sh

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

pgbench -i 'host=odyssey port=6432 user=postgres dbname=postgres'


psql 'host=odyssey port=6432 user=postgres dbname=postgres' -c 'select 1' || {
    echo "error: failed to make query"
    ody-stop
    exit 1
}

pgbench 'host=odyssey port=6432 user=postgres dbname=postgres' -T 20 -j 4 -c 16 --no-vacuum --progress 1 || {
    echo "error: failed to make pgbench query"
    ody-stop
    exit 1
}

ody-stop

