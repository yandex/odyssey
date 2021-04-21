#!/bin/bash
#kill -9 $(ps aux | grep odyssey | awk '{print $2}')
for _ in $(seq 1 40)
do
    sleep 0.1

    for __ in $(seq 1 10); do
            PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres2 -Uuser3 -c 'select 3' &
            PGPASSWORD=lolol psql -h localhost -p6432 -dpostgres2 -Uuser3 -c 'select pg_sleep(1)' &
    done

done
