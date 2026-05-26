#!/usr/bin/env bash

set -eux

PG_REGRESS=$(pg_config --libdir)/postgresql/pgxs/src/test/regress/pg_regress
REGRESS_DIR=/home/postgres/regress

PGDIR=/home/postgres/pgdir
PGDIRREPL1=/home/postgres/pgrepl1
PGDIRREPL2=/home/postgres/pgrepl2

do_test() {
    local name=$1
    local port=$2
    local user=$3

    export ASAN_OPTIONS="log_path=/asan-output.log fast_unwind_on_malloc=0"
    export TSAN_OPTIONS="log_path=/tsan-output.log"
    odyssey ${REGRESS_DIR}/conf/${name}

    local it=0
    until (( it == 50 )) || pg_isready -h localhost -p ${port} -U postgres -d postgres; do
        echo "Wait for odyssey..."
        echo "$(( it++ ))"
        sleep 0.1
    done
    (( it < 50 ))

    PGTEST_PORT="${port}" PGTEST_DIR="${REGRESS_DIR}/tests/${name}" PGTEST_USER="${user}" PGTEST_DB=postgres ${PG_REGRESS} \
        --debug \
        --host=localhost \
        --port=${port} \
        --user=${user} \
        --dbname=postgres \
        --use-existing \
        --schedule=${REGRESS_DIR}/schedule/${name} \
        --inputdir=${REGRESS_DIR}/tests/${name} \
        --outputdir=${REGRESS_DIR}/tests/${name} || {
            echo "====== DIFFS ======="
            cat ${REGRESS_DIR}/tests/${name}/regression.diffs || true

            echo "====== OUT ======="
            cat ${REGRESS_DIR}/tests/${name}/regression.out || true

            ody-stop
            exit 1
        }

    ody-stop
}

pg_ctl stop -D ${PGDIRREPL1} || true
pg_ctl stop -D ${PGDIRREPL2} || true
pg_ctl stop -D ${PGDIR} || true

rm -rf ${PGDIR} || true
rm -rf ${PGDIRREPL1} || true
rm -rf ${PGDIRREPL2} || true

initdb -D ${PGDIR}
pg_ctl start -D ${PGDIR} -l ${PGDIR}/log.txt

psql -p 5432 -c "ALTER SYSTEM SET wal_level = replica;"
psql -p 5432 -c "ALTER SYSTEM SET max_wal_senders = 10;"
pg_ctl restart -D ${PGDIR}

pg_basebackup -D ${PGDIRREPL1} -p 5432 -R
echo "port = 5433" >> ${PGDIRREPL1}/postgresql.conf
pg_ctl start -D ${PGDIRREPL1} -l ${PGDIRREPL1}/log.txt

pg_basebackup -D ${PGDIRREPL2} -p 5432 -R
echo "port = 5434" >> ${PGDIRREPL2}/postgresql.conf
pg_ctl start -D ${PGDIRREPL2} -l ${PGDIRREPL2}/log.txt

do_test simple 6432 suser
do_test simple 6432 tuser

do_test broken_conn 6432 tuser
