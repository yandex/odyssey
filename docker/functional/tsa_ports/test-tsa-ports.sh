#!/bin/bash

set -ex

# TODO: create more cool tests

/usr/bin/odyssey /tsa_ports/odyssey.conf
sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select pg_is_in_recovery()' | grep 't' || {
    echo 'exptected to port 6432 return connect to replica'
    exit 1
}

ody-stop

/usr/bin/odyssey /tsa_ports/odyssey.conf
sleep 1

psql 'host=localhost port=6433 user=postgres dbname=postgres' -c 'select pg_is_in_recovery()' | grep 'f' || {
    echo 'exptected to port 6433 return connect to master'
    exit 1
}

ody-stop