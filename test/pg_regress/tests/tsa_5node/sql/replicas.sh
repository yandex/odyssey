#!/usr/bin/env bash

# stop/start all replicas (PGDIRREPL* are exported by entrypoint.sh)

set -eu

cmd=$1

for dir in "${PGDIRREPL1}" "${PGDIRREPL2}" "${PGDIRREPL3}" "${PGDIRREPL4}"; do
    case "${cmd}" in
        stop)
            pg_ctl stop -D "${dir}" -m fast -s -w
            ;;
        start)
            pg_ctl start -D "${dir}" -l "${dir}/log.txt" -s -w
            ;;
        *)
            echo "usage: $0 stop|start" >&2
            exit 1
            ;;
    esac
done
