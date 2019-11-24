source ${0%/*}/environment.sh

# Try to stop odyssey
kill -9 `cat $ODYSSEY_PID_FILE 2>/dev/null` > /dev/null 2>&1

# Try to stop postgresql
pg_ctl -D $PGDATA stop > /dev/null 2>&1

# Remove tests artifacts
rm -rf $TEST_DATA
