#!/usr/bin/env bash

set -ex

odyssey /tests/reload/conf.conf
sleep 1

psql 'host=localhost port=6432 dbname=postgres user=postgres' -c 'select 1' || {
    cat /var/log/odyssey.log
    exit 1
}

sed -i 's/host\ "localhost"//g' /tests/reload/conf.conf
echo 'unix_socket_dir "/var/run/postgresql"' >> /tests/reload/conf.conf
echo 'unix_socket_mode "0777"' >> /tests/reload/conf.conf

kill -s HUP $(pidof odyssey)

psql 'host=localhost port=6432 dbname=postgres user=postgres' -c 'select 1' || {
    cat /var/log/odyssey.log
    exit 1
}

ody-stop
