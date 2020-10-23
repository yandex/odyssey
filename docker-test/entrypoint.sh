#!/bin/bash
set -ex
/usr/bin/pg_ctlcluster 13 main restart
psql -h localhost -p 5432 -U postgres -c 'SELECT 1 AS ok'

/ody-intergration-test


