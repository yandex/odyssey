set -ex

exit 0

ody-start

jdbc_test_dir=/jdbc/jdbc-spqr

git clone --depth 1 https://github.com/pg-sharding/jdbc-spqr.git $jdbc_test_dir
docker build -t jdbc-tests $jdbc_test_dir
rm -rf $jdbc_test_dir
docker run \
    -e PG_HOST='odyssey' \
    -e PG_PORT=6432 \
    -e PG_USER='spqr-console' \
    -e PG_DATABASE='spqr-console' \
    --network=odyssey_od_net jdbc-tests --skip-sharding-setup

ody-stop
