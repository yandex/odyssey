#!/bin/bash

set -e

sleep 1

for run in {1..1000}; do
    PGPASSWORD=lolol psql -h localhost -p6432 -dconsole -Uuser1 -c 'show clients' 1>/dev/null
done
