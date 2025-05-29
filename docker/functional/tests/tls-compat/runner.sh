#!/bin/bash -x

set -ex

pushd /tests/tls-compat/

openssl genrsa -out root.key 2048
openssl req -new -key root.key -out root.csr -nodes -subj "/CN=odyssey-test-cn"
openssl x509 -req -days 2 -in root.csr -signkey root.key -out root.pem

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA root.pem -CAkey root.key -CAcreateserial -out server.pem -days 2

openssl genrsa -out auth_query_user_md5.key 2048
openssl req -new -key auth_query_user_md5.key -out auth_query_user_md5.csr -nodes -subj "/CN=auth_query_user_md5"
openssl x509 -req -in auth_query_user_md5.csr -CA root.pem -CAkey root.key -CAcreateserial -out auth_query_user_md5.pem -days 2

popd

echo "ssl packages:"
dpkg -l | grep ssl | cat

/usr/bin/odyssey /tests/tls-compat/config.conf

# Check cert authentication
for _ in $(seq 1 100); do
  psql "host=localhost port=6432 user=auth_query_user_md5 dbname=auth_query_db sslmode=verify-full sslrootcert=/tests/tls-compat/root.pem sslkey=/tests/tls-compat/auth_query_user_md5.key sslcert=/tests/tls-compat/auth_query_user_md5.pem connect_timeout=500" -c "SELECT 1" || exit 1
done

# Check that parallel handshakes works well
mkdir -p /tls-compat/runs

for i in $(seq 1 500); do
  psql "host=localhost port=6432 user=auth_query_user_scram_sha_256 dbname=auth_query_db password=passwd sslmode=verify-full sslrootcert=/tests/tls-compat/root.pem connect_timeout=500" -c "SELECT 1" 1>/tls-compat/runs/run_${i} 2>&1 &
done

for _ in $(seq 1 500); do
  wait -n || {
    exit 1
  }
done;

# Check some read-only load will work with tls
pgbench 'host=localhost port=6432 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/tests/tls-compat/root.pem' -i -s 20
pgbench 'host=localhost port=6432 user=postgres dbname=postgres sslmode=verify-full sslrootcert=/tests/tls-compat/root.pem' -j 2 -c 10 --select-only --no-vacuum --progress 1 -T 60

ody-stop
