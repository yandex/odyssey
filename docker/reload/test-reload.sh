#!/bin/bash -x

/usr/bin/odyssey /etc/odyssey/reload-config.conf

make -j 2 -f /reload/Makefile || {
    echo "ERROR: simultaneous reloads with 'show clients' failed"

    cat /var/log/odyssey.log
    echo "

    "
    cat /var/log/postgresql/postgresql-16-main.log

    exit 1
}

ody-stop
