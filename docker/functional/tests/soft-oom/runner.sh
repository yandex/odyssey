#!/bin/bash -x

set -eux

/usr/bin/odyssey /tests/soft-oom/odyssey.conf
sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select 42' || {
    echo "memory eater didn't start yet, query should work"
    cat /var/log/odyssey.log
    exit 1
}

/tests/soft-oom/memory-eater &
sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select 42' || {
    echo "memory eater didn't allocate private memory, limit wasn't reached"
    cat /var/log/odyssey.log
    exit 1
}

for e in $(pgrep eater); do
    # each process of eater allocates 20mb per USR1
    kill -s USR1 $e
done

sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=disable' -c 'select 42' && {
    echo "memory eater finish allocating memory, new connections must fail"
    cat /var/log/odyssey.log
    exit 1
}

ody-stop || {
    cat /var/log/odyssey.log
    exit 1
}
