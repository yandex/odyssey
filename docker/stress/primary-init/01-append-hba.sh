#!/bin/bash

set -eux

sed -i "s/host all all all scram-sha-256//g" "$PGDATA/pg_hba.conf"

echo "host replication replicator all md5" >> "$PGDATA/pg_hba.conf"
echo "host all all all scram-sha-256" >> "$PGDATA/pg_hba.conf"
