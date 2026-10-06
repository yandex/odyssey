#!/bin/bash -x

set -e

pushd /tests/tls-workers/

openssl genrsa -out root.key 2048
openssl req -new -key root.key -out root.csr -nodes -subj "/CN=odyssey-test-cn"
openssl x509 -req -days 2 -in root.csr -signkey root.key -out root.pem

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA root.pem -CAkey root.key -CAcreateserial -out server.pem -days 2

popd

/usr/bin/odyssey /tests/tls-workers/config.conf
sleep 1

CONN='host=localhost port=6432 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/tests/tls-workers/root.pem'

psql "$CONN" -c 'select 1' || exit 1

# clients stuck in handshake must not block handshakes of other clients
python3 /tests/tls-workers/silent_clients.py 20 4 &
silent=$!
sleep 1

timeout 2 psql "$CONN" -c 'select 1' || {
    echo "handshake is blocked by silent clients"
    exit 1
}

wait $silent

pgbench "$CONN" -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 10 --connect || exit 1

ody-stop || exit 1
