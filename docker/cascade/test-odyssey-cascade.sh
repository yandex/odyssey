#!/bin/bash

set -ex

pushd /cascade/

rm -f allCA*
rm -f root*
rm -f gateway*

openssl genrsa -out allCA.key 2048
openssl req -new -key allCA.key -out allCA.csr -nodes -subj "/CN=odyssey-test-cn"
openssl x509 -req -days 2 -in allCA.csr -signkey allCA.key -out allCA.pem

openssl genrsa -out root.key 2048
openssl req -new -key root.key -out root.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in root.csr -CA allCA.pem -CAkey allCA.key -CAcreateserial -out root.pem -days 2

openssl genrsa -out gateway.key 2048
openssl req -new -key gateway.key -out gateway.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in gateway.csr -CA allCA.pem -CAkey allCA.key -CAcreateserial -out gateway.pem -days 2

popd

/usr/bin/odyssey /cascade/odyssey-root1.conf
/usr/bin/odyssey /cascade/odyssey-root2.conf
/usr/bin/odyssey /cascade/odyssey-gateway.conf

sleep 1

psql 'host=localhost port=6432 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/cascade/allCA.pem' -c 'select 1' || {
    echo "select 1 for postgres:postgres on root1 should work with tls"
    exit 1
}

psql 'host=localhost port=6433 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/cascade/allCA.pem' -c 'select 1' || {
    echo "select 1 for postgres:postgres on root2 should work with tls"
    exit 1
}

psql 'host=localhost port=7432 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/cascade/allCA.pem' -c 'select 1' || {
    echo "select 1 for postgres:postgres on gateway should work with tls"
    exit 1
}

pgbench 'host=localhost port=6432 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/cascade/allCA.pem' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 10 || {
    echo "pgbench should work on root odyssey"
    exit 1
}

pgbench 'host=localhost port=7432 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/cascade/allCA.pem' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 10 || {
    echo "pgbench should work on gateway odyssey"
    exit 1
}

sleep 1

ody-stop