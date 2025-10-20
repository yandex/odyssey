#!/bin/sh

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

./odyssey "./config.conf"

psql 'host=localhost port=6432 user=postgres dbname=postgres' -c 'select 1'

ody-stop

if ps aux | grep -q '[o]dyssey'; then
  echo "Can't finish odyssey after sigterm"
fi