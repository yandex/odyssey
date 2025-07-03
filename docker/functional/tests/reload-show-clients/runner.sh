#!/bin/bash -x

/usr/bin/odyssey /tests/reload-show-clients/config.conf

make -j 2 -f /tests/reload-show-clients/Makefile || {
    echo "ERROR: simultaneous reloads with 'show clients' failed"

    cat /var/log/odyssey.log
    echo "

    "
    cat /var/log/postgresql/postgresql-16-main.log

    exit 1
}

sleep 2

# there must be exactly one watchdog coroutine after config reload(s)
# function od_storage_watchdog_watch is defined in storage.c
if [ $(gdb -q --pid $(pidof odyssey) --batch -ex 'source /gdb.py' -ex 'info mmcoros' | grep -c 'od_storage_watchdog_watch') -ne 1 ]
then
    echo "!!! expected only one lag polling coro after reloads !!!"
    exit 1
fi

ody-stop
