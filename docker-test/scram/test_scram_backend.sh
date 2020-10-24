
psql -h localhost -p 6432 -U backend_auth_with_incorrect_password -c "SELECT 1" scram_db > /dev/null 2>&1 && {
    echo "ERROR: successfully backend auth with incorrect password"
    exit 1
}

psql -h localhost -p 6432 -U backend_auth_with_correct_password -c "SELECT 1" scram_db > /dev/null 2>&1 || {
    echo "ERROR: failed backend auth with correct password"
    exit 1
}
