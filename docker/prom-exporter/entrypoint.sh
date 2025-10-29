#!/bin/sh

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

until pg_isready -h odyssey -p 6432 -U postgres -d postgres; do
  echo "Wait for odyssey..."
  sleep 1
done

pgbench -i 'host=odyssey port=6432 user=postgres dbname=postgres'

psql 'host=odyssey port=6432 user=postgres dbname=postgres' -c 'select 1' || {
    echo "error: failed to make query"
    exit 1
}

pgbench 'host=odyssey port=6432 user=postgres dbname=postgres' -T 10 -j 4 -c 16 --no-vacuum --progress 1 || {
    echo "error: failed to make pgbench query"
    exit 1
}

sleep 1

./exporter --odyssey.connectionString="host=odyssey port=6432 user=console dbname=console sslmode=disable" &

sleep 1

curl tester:9876/metrics -s | grep 'odyssey_' | grep -v '#' | grep -v 'odyssey_exporter_build_info' | grep -v 'odyssey_version_info' | grep -v 'odyssey_pool_postgres_postgres_bytes_received' | grep -v 'odyssey_pool_postgres_postgres_bytes_sent' > result.out

echo EXPECTED START
cat expected.out
echo EXPECTED END
echo
echo RESULT START
cat result.out
echo RESULT END

diff expected.out result.out
