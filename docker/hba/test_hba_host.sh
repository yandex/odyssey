
PGPASSWORD=correct_password psql -h localhost -p 6432 -U user_allow -c "SELECT 1" hba_db > /dev/null 2>&1 || {
    echo "ERROR: failed auth with hba trust, correct password and plain password in config"
    exit 1
}

PGPASSWORD=incorrect_password psql -h localhost -p 6432 -U user_allow -c "SELECT 1" hba_db > /dev/null 2>&1 && {
    echo "ERROR: successfully auth with hba trust, but incorrect password"
    exit 1
}

PGPASSWORD=correct_password psql -h localhost -p 6432 -U user_reject -c "SELECT 1" hba_db > /dev/null 2>&1 && {
    echo "ERROR: successfully auth with hba reject"
    exit 1
}

PGPASSWORD=correct_password psql -h localhost -p 6432 -U user_unknown -c "SELECT 1" hba_db > /dev/null 2>&1 && {
    echo "ERROR: successfully auth without hba rule"
    exit 1
}

exit 0