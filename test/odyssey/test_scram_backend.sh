source ${0%/*}/environment.sh

if test $PGVERSION -lt 10; then
    echo "WARNING: SCRAM Authentication is not supported, skip"
    exit 0
fi

psql -h $ODYSSEY_HOST -p $ODYSSEY_PORT -U backend_auth_with_incorrect_password -c "SELECT 1" scram_db > /dev/null 2>&1 && {
    echo "ERROR: successfully backend auth with incorrect password"
    exit 1
}

psql -h $ODYSSEY_HOST -p $ODYSSEY_PORT -U backend_auth_with_correct_password -c "SELECT 1" scram_db > /dev/null 2>&1 || {
    echo "ERROR: failed backend auth with correct password"
    exit 1
}
