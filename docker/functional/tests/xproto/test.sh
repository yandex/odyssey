set -ex

ody-start

git clone --depth 1 https://github.com/pg-sharding/spqr.git /tests/xproto/spqr
cp /tests/xproto/spqr/test/xproto/proto_test.go /tests/xproto/proto_test.go
rm -rf /test/xproto/spqr
docker build -t xproto-tests /tests/xproto
docker run -e POSTGRES_HOST=odyssey -e POSTGRES_PORT=6432 -e POSTGRES_DB=xproto_db -e POSTGRES_USER=xproto --network=odyssey_od_net xproto-tests

ody-stop
