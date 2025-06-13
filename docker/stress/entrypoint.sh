#!/usr/bin/env bash

# TODO: create fuzzing and stress tests here

set -eux

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

until pg_isready -h replica -p 5432 -U postgres -d postgres; do
  echo "Wait for replica..."
  sleep 1
done

/odyssey /odyssey.conf
sleep 1

pgbench 'host=localhost port=6432 user=suser_rw dbname=postgres password=postgres' -i -s 20

# psql 'host=localhost port=6432 user=suser_rw dbname=pgbench_branches password=postgres' -c 'GRANT ALL ON DATABASE pgbench_branches TO tuser';
# psql 'host=localhost port=6432 user=suser_rw dbname=pgbench_tellers password=postgres' -c 'GRANT ALL ON DATABASE pgbench_tellers TO tuser';
# psql 'host=localhost port=6432 user=suser_rw dbname=pgbench_accounts password=postgres' -c 'GRANT ALL ON DATABASE pgbench_accounts TO tuser';
# psql 'host=localhost port=6432 user=suser_rw dbname=pgbench_history password=postgres' -c 'GRANT ALL ON DATABASE pgbench_history TO tuser';

pgbench 'host=localhost port=6432 user=suser dbname=postgres password=postgres' -j 2 -c 50 -S --no-vacuum --progress 1 -T 60 --max-tries=1 &
# pgbench 'host=localhost port=6432 user=tuser dbname=postgres password=postgres' -j 2 -c 50 -S --no-vacuum --progress 1 -T 60 --max-tries=1 &

pgbench 'host=localhost port=6432 user=suser_ro dbname=postgres password=postgres' -j 2 -c 50 -S --no-vacuum --progress 1 -T 60 --max-tries=1 &
# pgbench 'host=localhost port=6432 user=tuser_ro dbname=postgres password=postgres' -j 2 -c 50 -S --no-vacuum --progress 1 -T 60 --max-tries=1 &

pgbench 'host=localhost port=6432 user=suser_rw dbname=postgres password=postgres' -j 2 -c 50 --no-vacuum --progress 1 -T 60 --max-tries=1 &
# pgbench 'host=localhost port=6432 user=tuser_rw dbname=postgres password=postgres' -j 2 -c 50 --no-vacuum --progress 1 -T 60 --max-tries=1 &


for i in seq 6; do
    wait -n || {
        cat /odyssey.log
        exit 1
    }
done

pkill odyssey
