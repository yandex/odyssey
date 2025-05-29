#!/bin/bash

set -ex

# TODO: create more cool tests

/usr/bin/odyssey /tests/virtual_processing/odyssey.conf
sleep 1

mkdir /tests/virtual_processing/results/
cat /tests/virtual_processing/sql/virtual_show.sql | psql 'host=localhost port=6432 user=postgres dbname=postgres' --echo-all --quiet > /tests/virtual_processing/results/virtual_show.out 2>&1

diff /tests/virtual_processing/expected/virtual_show.out /tests/virtual_processing/results/virtual_show.out

ody-stop
