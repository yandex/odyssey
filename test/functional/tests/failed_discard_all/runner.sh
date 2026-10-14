#!/bin/bash

set -ex

/usr/bin/odyssey /tests/failed_discard_all/odyssey.conf
sleep 1

# psql 18 is needed for \parse and \bind_named
out=$(/usr/lib/postgresql/18/bin/psql -X -At \
    'host=localhost port=6432 user=postgres dbname=postgres' \
    -f /tests/failed_discard_all/failed_discard_all.sql 2>&1) || true

echo "$out"

if [ "$(echo "$out" | tail -n 1)" != "42" ]; then
    echo "prepared statement was lost after a failed DISCARD ALL"
    cat /var/log/odyssey.log
    ody-stop
    exit 1
fi

ody-stop
