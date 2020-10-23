#!/bin/bash
#kill -9 $(ps aux | grep odyssey | awk '{print $2}')
sleep 1

#ody-start

for i in `seq 1 10`
do
    sleep 0.1
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &

    #ody-restart
    ps uax | grep odys

    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
    psql -h 0.0.0.0 -p 6432 -c 'select pg_sleep(1)' &
    psql -h localhost -p 6432 -c 'select 1' &
done

sleep 2

kill $(pgrep odyssey)
