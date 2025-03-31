#!/bin/bash -x

set -ex

pushd /tls-compat/

openssl genrsa -out root.key 2048
openssl req -new -key root.key -out root.csr -nodes -subj "/CN=odyssey-test-cn"
openssl x509 -req -days 2 -in root.csr -signkey root.key -out root.pem

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA root.pem -CAkey root.key -CAcreateserial -out server.pem -days 2

openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in client.csr -CA root.pem -CAkey root.key -CAcreateserial -out client.pem -days 2

popd

echo "ssl packages:"
dpkg -l | grep ssl

/usr/bin/odyssey /tls-compat/config.conf
sleep 1

psql "host=localhost port=6432 user=auth_query_user_scram_sha_256 dbname=auth_query_db password=passwd sslmode=verify-full sslrootcert=/tls-compat/root.pem" -c "SELECT 1" || {
    echo "scram + tls failed"
    exit 1
}

psql "host=localhost port=6432 user=auth_query_user_md5 dbname=auth_query_db password=passwd sslmode=verify-full sslrootcert=/tls-compat/root.pem" -c "SELECT 1" || {
    echo "md5 + tls failed"
    exit 1
}

ody-stop
