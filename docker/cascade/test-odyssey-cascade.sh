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

mkdir -p /tmp/gateway
mkdir -p /tmp/root1
mkdir -p /tmp/root2

/usr/bin/odyssey /cascade/odyssey-root1.conf
sleep 1

/usr/bin/odyssey /cascade/odyssey-root2.conf
sleep 1

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

pgbench 'host=localhost port=7432 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/cascade/allCA.pem' -j 10 -c 500 --select-only --no-vacuum --progress 1 -T 10 || {
    echo "pgbench should work on gateway odyssey"
    exit 1
}

root1_client_processed=`cat /var/log/odyssey.root1.log | grep -oP 'clients_processed: \d+' | tail -n 1 | grep -oP '\d+'`
root2_client_processed=`cat /var/log/odyssey.root2.log | grep -oP 'clients_processed: \d+' | tail -n 1 | grep -oP '\d+'`

python3 -c 'import sys; \
root1 = int(sys.argv[-1]); \
root2 = int(sys.argv[-2]); \
diff = abs(root1 - root2); \
mean = abs(root1 + root2) / 2; \
threshold = 0.1 * mean; \
exit(0 if diff < threshold else 1)' $root1_client_processed $root2_client_processed || {
    echo "connects should be distributed near to equals between roots"
    exit 1
}

sleep 1

ody-stop