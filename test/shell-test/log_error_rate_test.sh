#!/bin/bash
#kill -9 $(ps aux | grep odyssey | awk '{print $2}')
sleep 1

#ody-start
#./odyssey ./ody.conf

for _ in $(seq 1 4000000)
do
    for __ in $(seq 1 3000); do
        psql -U user1 -d postgres -h localhost -p 6432 -c 'SELECT 1/0' &
        psql -U user1 -d postgres -h localhost -p 6432 -c 'select pg_sleep(0.01)' &
    done
done
