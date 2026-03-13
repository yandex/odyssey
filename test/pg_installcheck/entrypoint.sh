#!/usr/bin/env bash

set -eux

$PGINSTALL/bin/pg_ctl -D $PGDATA -l pg_logfile stop || true
rm -rf $PGDATA || true

$PGINSTALL/bin/initdb $PGDATA

cat > $PGDATA/pg_hba.conf <<EOF
local   all             all                                     trust
host    all             all             all                     trust
host    all             all             all                     trust
local   replication     all                                     trust
host    replication     all             all                     trust
host    replication     all             all                     trust
EOF

echo "listen_addresses = '*'" >> $PGDATA/postgresql.conf

$PGINSTALL/bin/pg_ctl -D $PGDATA -l pg_logfile start

until $PGINSTALL/bin/pg_isready -h localhost -p 5432 -U postgres -d postgres; do
  echo "Waiting for postgres to up..."
  sleep 1
done

until $PGINSTALL/bin/pg_isready -h odyssey -p 6432 -U postgres -d postgres; do
  echo "Waiting for odyssey to up..."
  sleep 1
done

# PGHOST=localhost PGPORT=5432 make -C $PGSRC installcheck

PGHOST=odyssey PGPORT=6432 make -C $PGSRC installcheck
