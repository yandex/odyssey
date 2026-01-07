#!/bin/bash

set -ex

# TODO: create more cool tests

/usr/bin/odyssey /tests/query_processing/odyssey.conf
sleep 1

mkdir /tests/query_processing/results/
cat /tests/query_processing/sql/virtual_show.sql | psql 'host=localhost port=6432 user=postgres dbname=postgres' --echo-all --quiet > /tests/query_processing/results/virtual_show.out 2>&1
cat /tests/query_processing/sql/application_name.sql | psql 'host=localhost port=6432 user=postgres dbname=postgres' --echo-all --quiet > /tests/query_processing/results/application_name.out 2>&1

diff /tests/query_processing/expected/application_name.out /tests/query_processing/results/application_name.out
diff /tests/query_processing/expected/virtual_show.out /tests/query_processing/results/virtual_show.out

ody-stop
