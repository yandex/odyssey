#!/bin/sh

set -ex

until pg_isready -h primary -p 5432 -U postgres -d postgres; do
  echo "Wait for primary..."
  sleep 0.1
done

CONN='-h odyssey -p 6432 -U postgres -d postgres'
REPLSLOTNAME='test_slot'

until pg_isready $CONN; do
  echo "Wait for odyssey..."
  sleep 0.1
done

lsn_ge() {
    local left="$1"
    local right="$2"

    result="$(psql $CONN -Atqc "select pg_lsn('$left') >= pg_lsn('$right')")"
    [ "$result" = "t" ]
}

get_confirmed_flush_lsn() {
    local slotname="$1"
    psql $CONN -Atqc "
        select coalesce(confirmed_flush_lsn::text, '')
        from pg_replication_slots
        where slot_name = '$slotname'
    "
}

wait_for_confirmed_flush_lsn_ge() {
    local target_lsn="$1"
    local slotname="$2"

    for _ in $(seq 1 30); do
        current_lsn="$(get_confirmed_flush_lsn $slotname)"
        if [ -n "$current_lsn" ] && lsn_ge "$current_lsn" "$target_lsn"; then
            echo "confirmed_flush_lsn reached target: $current_lsn >= $target_lsn"
            return 0
        fi
        sleep 1
    done

    echo "confirmed_flush_lsn did not reach target $target_lsn" >&2
    current_lsn="$(get_confirmed_flush_lsn $slotname)"
    echo "current confirmed_flush_lsn: $current_lsn" >&2
    exit 1
}

do_logical_repl_test() {
    psql $CONN -c "select pg_drop_replication_slot('$REPLSLOTNAME')" || true
    psql $CONN -c 'drop table if exists z'
    psql $CONN -c 'create table z(x int, y int, z int)'

    pg_recvlogical $CONN --slot $REPLSLOTNAME --create-slot -P test_decoding

    psql $CONN -c 'insert into z values(1, 2, 3)'
    psql $CONN -c 'insert into z values(4, 5, 6)'
    psql $CONN -c 'insert into z values(7, 8, 9)'

    echo "=== START 1 ===" >/logical_actual.out
    pg_recvlogical $CONN --slot $REPLSLOTNAME --start -f - -o include-xids=0 >>/logical_actual.out 2>&1 &
    pid=$!

    psql $CONN -c 'insert into z values(10, 11, 12)'
    psql $CONN -c 'insert into z values(13, 14, 15)'
    psql $CONN -c 'insert into z values(16, 17, 18)'

    target_lsn="$(psql $CONN -Atqc "select pg_current_wal_lsn()")"
    wait_for_confirmed_flush_lsn_ge $target_lsn $REPLSLOTNAME

    kill -INT $pid
    wait $pid || {
        echo 'recvlogical failed'
        exit 1
    }
    unset pid

    psql $CONN -c 'insert into z values(19, 20, 21)'
    psql $CONN -c 'insert into z values(22, 23, 24)'
    psql $CONN -c 'insert into z values(25, 26, 27)'

    echo "=== START 2 ===" >>/logical_actual.out
    pg_recvlogical $CONN --slot $REPLSLOTNAME --start -f - -o include-xids=0 >>/logical_actual.out 2>&1 &
    pid=$!

    psql $CONN -c 'insert into z values(28, 29, 30)'
    psql $CONN -c 'insert into z values(31, 32, 33)'
    psql $CONN -c 'insert into z values(34, 35, 36)'

    target_lsn="$(psql $CONN -Atqc "select pg_current_wal_lsn()")"
    wait_for_confirmed_flush_lsn_ge $target_lsn $REPLSLOTNAME

    kill -INT $pid
    wait $pid || {
        echo 'recvlogical failed'
        exit 1
    }
    unset pid

    diff /logical_actual.out /logical_expected.out || {
        cat /logical_actual.out
        exit 1
    }
}

do_physical_repl_test() {
    psql $CONN -c "select pg_drop_replication_slot('$REPLSLOTNAME')" || true

    local backup_dir='/tmp/backup'
    rm -rf $backup_dir

    pg_basebackup -h odyssey -p 6432 -U postgres -D "$backup_dir" -X stream --checkpoint=fast -Fp -R -C -S "$REPLSLOTNAME" -v || {
        echo "pg_basebackup failed"
        exit 1
    }

    test -f "$backup_dir/PG_VERSION"
    test -d "$backup_dir/base"
    test -d "$backup_dir/global"
    test -f "$backup_dir/standby.signal"
    test -f "$backup_dir/postgresql.auto.conf"

    grep -q "primary_conninfo" "$backup_dir/postgresql.auto.conf"

    psql $CONN -Atqc "select slot_name from pg_replication_slots where slot_name = '$REPLSLOTNAME' and slot_type = 'physical'" | grep -q "^$REPLSLOTNAME$"
}

do_logical_repl_test
do_physical_repl_test
