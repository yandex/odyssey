export PATH="$PATH:$(pg_config --bindir)"

export LANG=C

export TEST_DATA=odyssey/data

export PGDATA=$TEST_DATA/pg
export PGHOST=/tmp
export PGVERSION=`initdb --version | sed -nr 's/.* ([0-9]+).*/\1/p'`

export ODYSSEY=../sources/odyssey
export ODYSSEY_CONFIG=odyssey/config
export ODYSSEY_PID_FILE=$TEST_DATA/odyssey.pid
export ODYSSEY_HOST=127.0.0.1
export ODYSSEY_PORT=6432
