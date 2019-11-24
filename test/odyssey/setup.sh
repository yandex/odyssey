source ${0%/*}/environment.sh

if [ -z "$CI" ]; then
    SETUP_LOG=$TEST_DATA/setup.log
else
    SETUP_LOG=/dev/stdout
fi

# Pre-check requirements
which initdb > /dev/null && which pg_ctl > /dev/null || {
    echo "ERROR: postgresql not found, to run test please install postgresql"
    exit 1
}

# Teardown previous run, if needed
if [ -d $TEST_DATA ]; then
    bash ${0%/*}/teardown.sh
fi

# Initialize pgdata directory
mkdir -p $PGDATA || exit 1
initdb -D $PGDATA >> $SETUP_LOG 2>&1 || {
    echo "ERROR: 'initdb' failed, examine the $SETUP_LOG"
    exit 1
}

# Redefine postgresql unix socket directory
echo "unix_socket_directories = '$PGHOST'" >> $PGDATA/postgresql.conf

# Set host based authentication rules
if test $PGVERSION -ge 10; then
    cat > $PGDATA/pg_hba.conf <<-EOF
local  scram_db  all                scram-sha-256
host   scram_db  all  127.0.0.1/32  scram-sha-256
EOF
else
    cat > $PGDATA/pg_hba.conf < /dev/null
fi

cat >> $PGDATA/pg_hba.conf <<-EOF
local  all       all                trust
host   all       all  127.0.0.1/32  trust
EOF

# Start postgresql
pg_ctl -D $PGDATA start >> $SETUP_LOG 2>&1 || {
    echo "ERROR: 'pg_ctl start' failed, examine the $SETUP_LOG"
    exit 1
}

# Create databases
for database_name in db scram_db; do
    createdb $database_name >> $SETUP_LOG 2>&1 || {
        echo "ERROR: 'createdb $database_name' failed, examine the $SETUP_LOG"
        exit 1
    }
done

# Create users
if test $PGVERSION -ge 10; then
    psql -c "set password_encryption = 'scram-sha-256'; create user scram_user password 'scram_user_password';" db >> $SETUP_LOG 2>&1 || {
        echo "ERROR: users creation failed, examine the $SETUP_LOG"
        exit 1
    }
fi

# Start odyssey
$ODYSSEY $ODYSSEY_CONFIG >> $SETUP_LOG 2>&1 || {
    echo "ERROR: start odyssey failed, examine the $SETUP_LOG"
    exit 1
}
