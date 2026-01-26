#!/bin/bash

set -ex

check_file() {
    local name=$1
    local options=$2

    export PGOPTIONS=$options
    cat /tests/pgoptions/sql/$name.sql | psql 'host=localhost port=6432 user=useropt dbname=postgres' --echo-all --quiet > /tests/pgoptions/results/$name.out 2>&1 || {
        cat /tests/pgoptions/results/$name.out
        exit 1
    }

    diff /tests/pgoptions/expected/$name.out /tests/pgoptions/results/$name.out || {
        cat /tests/pgoptions/results/$name.out
        xxd /tests/pgoptions/results/$name.out
        exit 1
    }
}

/usr/bin/odyssey /tests/pgoptions/odyssey.conf
sleep 1

mkdir -p /tests/pgoptions/results/

# check_file invalid_parameter '-c search_pathh=invalid_name'
check_file role '-c role=zz'
check_file several_params ' -c role=zz  --search_path=public   -c statement_timeout=1337   --work-mem=765MB    '
check_file search_paths '--search-path="$user",public,popis'
check_file search_path_injection '--search-path=public;create/**/table/**/zzz()'

ody-stop
