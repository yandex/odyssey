#!/bin/bash
#kill -9 $(ps aux | grep odyssey | awk '{print $2}')
sleep 1

#ody-start

for _ in $(seq 1 10)
do
    sleep 0.1

    for __ in $(seq 1 10); do
      psql -U postgres -d postgres -h localhost -p 6432 -c 'select 1' &
      psql -U postgres -d postgres -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    done

    #ody-restart
    ps uax | grep odys


    for __ in $(seq 1 30); do
      psql -U postgres -d postgres -h localhost -p 6432 -c 'select 1' &
      psql -U postgres -d postgres -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    done

done
