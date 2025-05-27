#!/bin/bash

set -ex

# TODO: create more cool tests

/usr/bin/odyssey /virtual_processing/odyssey.conf
sleep 1

mkdir /virtual_processing/results/
cat /virtual_processing/sql/virtual_show.sql | psql 'host=localhost port=6432 user=postgres dbname=postgres' --echo-all --quiet > /virtual_processing/results/virtual_show.out 2>&1

diff /virtual_processing/expected/virtual_show.out /virtual_processing/results/virtual_show.out

ody-stop