#!/bin/env bash

set -eux

openssl genrsa -out ca.key 2048
openssl req -new -key ca.key -out ca.csr -nodes -subj "/CN=odyssey-test-cn"
openssl x509 -req -days 365 -in ca.csr -signkey ca.key -out ca.crt

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365

openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr -nodes -subj "/CN=localhost"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365

rm ca.key ca.csr ca.srl
rm server.csr
rm client.csr
