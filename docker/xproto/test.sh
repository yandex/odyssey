git clone https://github.com/pg-sharding/spqr.git /xproto/spqr
cp /xproto/spqr/test/xproto/proto_test.go /xproto/proto_test.go
rm -rf /xproto/spqr
docker build -t xproto-tests /xproto
docker run -e POSTGRES_HOST=localhost -e POSTGRES_PORT=5432 -e POSTGRES_DB=xproto_db -e POSTGRES_USER=xproto --network=host xproto-tests