docker build -t gorm-tests /gorm/gorm-spqr
docker run -e DB_HOST='odyssey' -e DB_PORT=6432 -e DB_USER='spqr-console' -e DB_NAME='spqr-console' --network=odyssey_od_net gorm-tests