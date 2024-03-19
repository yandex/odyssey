git clone https://github.com/pg-sharding/gorm-spqr.git /gorm/gorm-spqr
docker build -t gorm-tests /gorm/gorm-spqr
rm -rf /gorm/gorm-spqr
docker run -e DB_HOST='odyssey' -e DB_PORT=6432 -e DB_USER='spqr-console' -e DB_NAME='spqr-console' -e EXTRA_PARAMS='client_encoding=UTF8' --network=odyssey_od_net gorm-tests