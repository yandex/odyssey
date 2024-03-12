# docker run -e DB_HOST='odyssey' -e DB_PORT=6432 -e DB_USER='spqr-console' -e DB_NAME='spqr-console' --network=odyssey_od_net gorm-tests
sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
COMPOSE_PROJECT_NAME=odyssey docker-compose -f /docker-compose-test.yml up gorm-test