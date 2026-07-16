#!/usr/bin/env bash

set -xu

PGDIR=/home/postgres/pgdir

pg_ctl stop -D ${PGDIR} -l ${PGDIR}/log.txt || true
rm -rf ${PGDIR} || true

initdb -D ${PGDIR}

cat >> "${PGDIR}/postgresql.conf" <<EOF
password_encryption = 'scram-sha-256'
listen_addresses = '*'
EOF

cat > "${PGDIR}/pg_hba.conf" <<EOF
# TYPE  DATABASE  USER          ADDRESS         METHOD
host    all       postgres      127.0.0.1/32    trust
host    all       postgres      ::1/128         trust

host    testdb    test_scram    127.0.0.1/32    scram-sha-256
host    testdb    test_scram    ::1/128         scram-sha-256
host    testdb    test_scram    0.0.0.0/0       scram-sha-256
EOF

pg_ctl start -D ${PGDIR} -l ${PGDIR}/log.txt || exit 1

until pg_isready -h localhost -p 5432 >/dev/null 2>&1; do
	sleep 0.2
done

psql -h localhost -U postgres -d postgres -c 'create database testdb' || exit 1
psql -h localhost -U postgres -d postgres -c "create role test_scram with password 'password' login" || exit 1

PGPASSWORD=password psql -h localhost -U test_scram -d testdb -c 'select 42' || exit 1
PGPASSWORD=not-password psql -h localhost -U test_scram -d testdb -c 'select 42' && {
    echo "successfully auth with incorrect password"
    exit 1
}

odyssey /home/postgres/odyssey.conf || exit 1
until pg_isready -h localhost -p 6432 >/dev/null 2>&1; do
	sleep 0.2
done

PGPASSWORD=password psql -h localhost -p 6432 -U test_scram -d testdb -c 'select 42' || {
	cat /home/postgres/odyssey.log
	exit 1
}
PGPASSWORD=not-password psql -h localhost -p 6432  -U test_scram -d testdb -c 'select 42' && {
    echo "successfully auth with incorrect password"
	cat /home/postgres/odyssey.log
    exit 1
}

ody-stop

exit 0
