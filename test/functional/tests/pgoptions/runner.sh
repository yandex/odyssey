#!/bin/bash

set -ex

export PGHOST=localhost PGPORT=6432 PGUSER=useropt PGDATABASE=postgres

check_file() {
    local name=$1
    local options=$2

    export PGOPTIONS=$options
    cat /tests/pgoptions/sql/$name.sql | psql -X --echo-all --quiet > /tests/pgoptions/results/$name.out 2>&1 || {
        cat /tests/pgoptions/results/$name.out
        exit 1
    }

    diff /tests/pgoptions/expected/$name.out /tests/pgoptions/results/$name.out || {
        cat /tests/pgoptions/results/$name.out
        xxd /tests/pgoptions/results/$name.out
        exit 1
    }
}

check_search_path() {
    check_file search_path_restore '-c search_path="odyssey\ test\ path",\ "odyssey_test_""quote",\ public'

    local long_path=public
    for i in {1..100}; do
        long_path+=,public
    done
    check_file search_path_long "-c search_path=$long_path"

    check_file search_path_unquoted_injection '-c search_path=public;CREATE/**/TABLE/**/ody_inj();--'
    check_file search_path_keyword '-c search_path=user'
}

/usr/bin/odyssey /tests/pgoptions/odyssey.conf
sleep 1

mkdir -p /tests/pgoptions/results/

# check_file invalid_parameter '-c search_pathh=invalid_name'
check_file role '-c role=zz'
check_file several_params ' -c role=zz  --search_path=public   -c statement_timeout=1337   --work-mem=765MB    '
check_file search_paths '--search-path="$user",\ public,\ popis'
check_file search_path_injection '--search-path="public;create/**/table/**/zzz()"'
check_file spqrguard '-c spqrguard.prevent_distributed_table_modify=on  -c spqrguard.prevent_reference_table_modify=off'

check_search_path
check_file search_path_complex '-c search_path="odyssey-test-path",\ "odyssey_кириллица",\ public'
version=$(PGOPTIONS='' psql -XAt -c 'SHOW server_version_num')
if (( version >= 180000 )); then
    check_file search_path_runtime '-c search_path=public'
fi
if PGOPTIONS='-c search_path=public$;\ SELECT\ 1\ AS\ x$' \
    psql -XAt -v ON_ERROR_STOP=1 -c 'SELECT 1' > /tests/pgoptions/results/rejected.out 2>&1; then
    cat /tests/pgoptions/results/rejected.out
    exit 1
fi

ody-stop

sed 's/smart_search_path_enquoting yes/smart_search_path_enquoting no/' \
    /tests/pgoptions/odyssey.conf > /tmp/odyssey-pgoptions.conf
/usr/bin/odyssey /tmp/odyssey-pgoptions.conf
sleep 1
check_search_path
check_file search_path_legacy '-c search_path=odyssey-legacy,public'

ody-stop
