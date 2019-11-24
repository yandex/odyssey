source ${0%/*}/environment.sh

if test $PGVERSION -lt 10; then
    echo "WARNING: SCRAM Authentication is not supported, skip"
    exit 0
fi

PGPASSWORD=incorrect_password psql -h $ODYSSEY_HOST -p $ODYSSEY_PORT -U frontend_auth_plain -c "SELECT 1" scram_db > /dev/null 2>&1 && {
    echo "ERROR: successfully auth with incorrect password and plain password in config"
    exit 1
}

PGPASSWORD=correct_password psql -h $ODYSSEY_HOST -p $ODYSSEY_PORT -U frontend_auth_plain -c "SELECT 1" scram_db > /dev/null 2>&1 || {
    echo "ERROR: failed auth with correct password and plain password in config"
    exit 1
}

PGPASSWORD=incorrect_password psql -h $ODYSSEY_HOST -p $ODYSSEY_PORT -U frontend_auth_scram_secret -c "SELECT 1" scram_db > /dev/null 2>&1 && {
    echo "ERROR: successfully auth with incorrect password and scram secret in config"
    exit 1
}

PGPASSWORD=correct_password psql -h $ODYSSEY_HOST -p $ODYSSEY_PORT -U frontend_auth_scram_secret -c "SELECT 1" scram_db > /dev/null 2>&1 || {
    echo "ERROR: failed auth with correct password and scram secret in config"
    exit 1
}
