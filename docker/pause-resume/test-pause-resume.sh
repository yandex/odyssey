#!/bin/bash

set -ex

/usr/bin/odyssey /pause-resume/config.conf
sleep 1

psql -h localhost -p 6432 -c 'select 1' -U user1 -d postgres
psql -h localhost -p 6432 -c 'select 1' -U postgres -d postgres

ody-stop
