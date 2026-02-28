#!/usr/bin/env bash

set -eux

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

# until pg_isready -h odyssey -p 6432 -U postgres -d postgres; do
#   echo "Wait for odyssey..."
#   sleep 1
# done

cd pgjdbc

cat > build.local.properties <<EOF
test.url.PGHOST=primary
test.url.PGPORT=5432
secondaryServer1=primary
secondaryPort1=5433
secondaryServer2=localhost
secondaryPort2=5434
test.url.PGDBNAME=test
user=test
password=test
privilegedUser=postgres
privilegedPassword=
sspiusername=testsspi
preparethreshold=5
sslpassword=sslpwd
EOF

./gradlew cleanTest && ./gradlew test
