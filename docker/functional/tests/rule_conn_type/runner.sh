#!/bin/bash -x

set -ex

pushd /tests/rule_conn_type/

rm -f root*
rm -f server*

openssl genrsa -out root.key 2048
openssl req -new -key root.key -out root.csr -nodes -subj "/CN=odyssey-test-cn"
openssl x509 -req -days 2 -in root.csr -signkey root.key -out root.pem

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA root.pem -CAkey root.key -CAcreateserial -out server.pem -days 2

popd

/usr/bin/odyssey /tests/rule_conn_type/odyssey.conf
sleep 1

# db1.user1 has host connection type - should work with ssl and without
psql 'host=localhost port=6432 user=user1 dbname=db1 sslmode=disable' -c 'select 42' || {
	echo 'db1.user1 should work without ssl'
	exit 1
}
psql 'host=localhost port=6432 user=user1 dbname=db1 sslmode=verify-full sslrootcert=/tests/rule_conn_type/root.pem' -c 'select 42' || {
	echo 'db1.user1 should work with ssl'
	exit 1
}

# tsa_db.user_ro has hostsll connection type - only connections with ssl should work
psql 'host=localhost port=6432 user=user_ro dbname=tsa_db sslmode=verify-full sslrootcert=/tests/rule_conn_type/root.pem' -c 'select 42' || {
	echo 'tsa_db.user_ro should work with ssl'
	exit 1
}
psql 'host=localhost port=6432 user=user_ro dbname=tsa_db sslmode=disable' -c 'select 42' && {
	echo 'tsa_db.user_ro should route only with ssl enabled'
	exit 1
}

# tsa_db.user_rw has hostnosll connection type - only connections without ssl should work
psql 'host=localhost port=6432 user=user_rw dbname=tsa_db sslmode=verify-full sslrootcert=/tests/rule_conn_type/root.pem' -c 'select 42' && {
	echo 'tsa_db.user_rw should not work with ssl'
	exit 1
}
psql 'host=localhost port=6432 user=user_rw dbname=tsa_db sslmode=disable' -c 'select 42' || {
	echo 'tsa_db.user_rw should route only with ssl disabled'
	exit 1
}

ody-stop
