#!/bin/bash -x

# check that transactional load works

set -exu

/usr/bin/odyssey /tests/transactional_load/odyssey.conf
sleep 1

# more connections than pool size
pgbench 'host=localhost port=6432 user=postgres dbname=postgres' -f /tests/transactional_load/f.sql --no-vacuum -j 4 -c 40 -T 20 --progress 1

ody-stop
