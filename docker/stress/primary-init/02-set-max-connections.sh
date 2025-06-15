#!/bin/bash

set -eux

sed -i "s/max_connections = 100/max_connections = 400/g" "$PGDATA/postgresql.conf"

