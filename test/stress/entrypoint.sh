#!/usr/bin/env bash

# TODO: create fuzzing tests here

set -eux

error_exit() {
  for i in /asan-output.log.*; do
    echo $i;
    cat $i;
  done
  exit 1
}

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

pgbench 'host=localhost port=6432 user=postgres dbname=postgres password=postgres' -i -s 20

DURATION=$((10 * 60))

./stress.sh -h localhost -p 6432 -u suser_rw -d postgres -t $DURATION -n suser_rw &
SUSER_RW_PID=$!
./stress.sh -h localhost -p 6432 -u tuser_rw -d postgres -t $DURATION -n tuser_rw &
TUSER_RW_PID=$!

./stress.sh -h localhost -p 6432 -u suser_ro -d postgres -t $DURATION -n suser_ro -r &
SUSER_RO_PID=$!
./stress.sh -h localhost -p 6432 -u tuser_ro -d postgres -t $DURATION -n tuser_ro -r &
TUSER_RO_PID=$!

./stress.sh -h localhost -p 6432 -u suser -d postgres -t $DURATION -n suser -r &
SUSER_PID=$!
./stress.sh -h localhost -p 6432 -u tuser -d postgres -t $DURATION -n tuser -r &
TUSER_PID=$!

wait $SUSER_RW_PID || error_exit
echo "[`date` entrypoint] suser_rw finished"
wait $TUSER_RW_PID || error_exit
echo "[`date` entrypoint] tuser_rw finished"

wait $SUSER_RO_PID || error_exit
echo "[`date` entrypoint] suser_ro finished"
wait $TUSER_RO_PID || error_exit
echo "[`date` entrypoint] tuser_ro finished"

wait $SUSER_PID || error_exit
echo "[`date` entrypoint] suser finished"
wait $TUSER_PID || error_exit
echo "[`date` entrypoint] tuser finished"

ps aux | head -n 1
ps aux | grep odyssey

ody-stop || error_exit
