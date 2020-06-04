source ${0%/*}/environment.sh

# Try to stop postgresql
pg_ctl -D $PGDATA stop > /dev/null 2>&1
