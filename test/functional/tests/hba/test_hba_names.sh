#!/bin/bash

set -euo pipefail

export PGPASSWORD=correct_password
export PGCONNECT_TIMEOUT=5
export PGSSLMODE=disable

host=$1
port=${PGPORT:-6432}

query() {
    psql -X -w -v ON_ERROR_STOP=1 -At -h "$host" -p "$port" \
        -d "$1" -U "$2" -c 'SELECT current_user, current_database()'
}

check_allowed() {
    local result
    result=$(query "$1" "$2")
    if [[ "$result" != 'user_allow|hba_db' ]]; then
        echo "ERROR: unexpected backend identity for $1/$2: $result"
        exit 1
    fi
    echo "OK: HBA allows $1/$2 via $host"
}

check_rejected() {
    local result
    if result=$(query "$1" "$2" 2>&1); then
        echo "ERROR: HBA allowed $1/$2"
        exit 1
    fi
    if [[ "$result" != *'host based authentication rejected'* ]]; then
        echo "ERROR: expected HBA rejection for $1/$2: $result"
        exit 1
    fi
    echo "OK: HBA rejects $1/$2 via $host"
}

check_allowed hba_default_db hba_allowed
check_rejected hba_default_db hba_denied
check_rejected hba_unknown_db hba_allowed
check_allowed hba_sameuser hba_sameuser
check_rejected hba_sameuser hba_otheruser
