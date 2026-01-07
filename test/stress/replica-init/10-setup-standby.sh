#!/bin/bash
set -eux

until pg_isready -h primary -p 5432 -U replicator -d postgres; do
  echo "Wait for primary..."
  sleep 1
done

pg_ctl -D "$PGDATA" -m fast -w stop

rm -rf "$PGDATA"/*

PGPASSWORD="postgres" pg_basebackup -h primary -D "$PGDATA" -U replicator -Fp -Xs -P -R

pg_ctl -D "$PGDATA" -m fast -w start
