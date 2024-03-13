# COMPOSE_PROJECT_NAME=odyssey docker-compose -f /docker-compose-test.yml up gorm-test
git clone https://github.com/pg-sharding/gorm-spqr.git
cd gorm-spqr/
docker build -t gorm-tests .
docker run -e DB_HOST='localhost' -e DB_PORT=6432 -e DB_USER='spqr-console' -e DB_NAME='spqr-console' --network=host gorm-tests
