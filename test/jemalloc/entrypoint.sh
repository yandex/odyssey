#!/usr/bin/env bash

set -eux

until pg_isready -h postgres -p 5432 -U postgres -d postgres; do
  echo "Waiting for postgres to up..."
  sleep 1
done

MALLOC_CONF=prof_leak:true,lg_prof_sample:0,prof_final:true LD_PRELOAD=${JEMALLOC_PATH}/lib/libjemalloc.so.2 /odyssey /odyssey.conf
sleep 1

pgbench 'host=localhost port=6432 user=postgres dbname=postgres password=postgres' -i -s 20
pgbench 'host=localhost port=6432 user=postgres dbname=postgres password=postgres' -T 30 --progress 1 --no-vacuum -j 4 -c 50 --select-only

ody-stop

${JEMALLOC_PATH}/bin/jeprof --show_bytes /odyssey jeprof.*

${JEMALLOC_PATH}/bin/jeprof --show_bytes --pdf /odyssey jeprof.* > /report.pdf