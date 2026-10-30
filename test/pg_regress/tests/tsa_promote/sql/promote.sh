#!/usr/bin/env bash

# promote scenario helpers (PGDIRREPL1..4 are exported by entrypoint.sh):
#   stop    - put the replica on 5434 down
#   promote - promote the replica on 5433 to a new master and wait
#             until odyssey notices the new endpoint state

set -eu

POLL="${PGTEST_PROMOTE_POLL:-5000}"

cmd=$1

case "${cmd}" in
stop)
    pg_ctl stop -D "${PGDIRREPL2}" -m fast -s -w
    ;;
promote)
    pg_ctl promote -D "${PGDIRREPL1}" -s -w

    # the promote is complete when the node leaves recovery
    i=0
    while [ "${i}" -lt 100 ]; do
        if psql -X -q -h localhost -p 5433 -U postgres -d postgres \
            -t -A -c 'select not pg_is_in_recovery()' 2>/dev/null \
            | grep -q t; then
            break
        fi
        sleep 0.1
        i=$((i + 1))
    done
    [ "${i}" -lt 100 ]

    # let odyssey refresh the endpoints state before the test goes on
    sleep "$((POLL / 1000 + 2))"
    ;;
*)
    echo "usage: $0 stop|promote" >&2
    exit 1
    ;;
esac
