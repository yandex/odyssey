#!/bin/bash

ODYSSEY_HOST=localhost
ODYSSEY_PORT=6432
PGUSER=pguser
PGDB=postgres
DURATION=600
READONLY=0

usage() {
  echo "Usage: $0 [-h host] [-p port] [-u user] [-d dbname] [-t duration_sec] [-n name] [-r]"
  exit 1
}

while getopts "h:p:u:d:t:n:r" opt; do
  case $opt in
    h) ODYSSEY_HOST="$OPTARG" ;;
    p) ODYSSEY_PORT="$OPTARG" ;;
    u) PGUSER="$OPTARG" ;;
    d) PGDB="$OPTARG" ;;
    t) DURATION="$OPTARG" ;;
    n) NAME="$OPTARG" ;;
    r) READONLY=1 ;;
    *) usage ;;
  esac
done

echo "[`date` $NAME] Stress test parameters:"
echo "[`date` $NAME] Host: $ODYSSEY_HOST"
echo "[`date` $NAME] Port: $ODYSSEY_PORT"
echo "[`date` $NAME] User: $PGUSER"
echo "[`date` $NAME] Db:   $PGDB"
echo "[`date` $NAME] Duration: $DURATION"
echo

STABLE_CLIENTS=10
STABLE_THREADS=2

WAVE_CLIENTS=40
WAVE_THREADS=2

UNSTABLE_CLIENTS=20
UNSTABLE_THREADS=2

PG_CONNECTION_STRING="host=$ODYSSEY_HOST port=$ODYSSEY_PORT user=$PGUSER dbname=$PGDB password=postgres"
PGBENCH_COMMON_OPTIONS="--no-vacuum --max-tries=1"
PGBENCH_OPTIONS='-c statement_timeout=10000'
if [[ $READONLY -eq 1 ]]; then
    PGBENCH_COMMON_OPTIONS="$PGBENCH_COMMON_OPTIONS --select-only"
fi

LOG_DIR="/stress_logs-$NAME"
mkdir -p "$LOG_DIR"

WAVE_RUN=30
WAVE_PAUSE=15

UNSTABLE_MIN=10
UNSTABLE_MAX=20

stable_load() {
    echo "[`date` $NAME] Starting stable load..."
    PGOPTIONS="$PGBENCH_OPTIONS" pgbench "$PG_CONNECTION_STRING" -c $STABLE_CLIENTS -j $STABLE_THREADS -T "$DURATION" $PGBENCH_COMMON_OPTIONS > "$LOG_DIR/stable.log" 2>&1 &
    local pid=$!
    wait $pid || {
        echo "[`date` $NAME] Stable run failed"
        exit 1
    }

    echo "[`date` $NAME] Stable load finished"
}

wave_load() {
    local begin=$(date +%s)
    local now=$begin
    local end=$((begin + DURATION))
    local i=1
    while [ $now -lt $end ]; do
        local left=$((end - now))
        local run=$WAVE_RUN; [ $left -lt $WAVE_RUN ] && run=$left

        echo "[`date` $NAME] Starting wave $i for $run seconds"

        PGOPTIONS="$PGBENCH_OPTIONS" pgbench "$PG_CONNECTION_STRING" -c $WAVE_CLIENTS -j $WAVE_THREADS -T "$run" $PGBENCH_COMMON_OPTIONS > "$LOG_DIR/wave-$i.log" 2>&1 &
        local pid=$!

        wait $pid || {
            echo "[`date` $NAME] Wave run failed"
            cat "$LOG_DIR/wave-$i.log"
            exit 1
        }

        echo "[`date` $NAME] Wave $i finished"

        if [ $now -lt $end ]; then
        local pause=$WAVE_PAUSE
        [ $((end - now - run)) -lt $pause ] && pause=$((end - now - run))
        [ $pause -gt 0 ] && sleep $pause
        fi

        now=$(date +%s)
        i=$((i+1))
    done
}

unstable_load() {
  local begin=$(date +%s)
  local now=$begin
  local end=$((begin + DURATION))
  local i=1
  while [ $now -lt $end ]; do
    local left=$((end - now))
    local kill_after=$(( RANDOM % ($UNSTABLE_MAX-$UNSTABLE_MIN+1) + $UNSTABLE_MIN ))
    [ $kill_after -gt $left ] && kill_after=$left
    echo "[`date` $NAME] Starting unstable $i"
    PGOPTIONS="$PGBENCH_OPTIONS" pgbench "$PG_CONNECTION_STRING" -c $UNSTABLE_CLIENTS -j $UNSTABLE_THREADS -T 100500 $PGBENCH_COMMON_OPTIONS > "$LOG_DIR/unstable_$i.log" 2>&1 &
    local pid=$!
    sleep $kill_after
    kill -9 $pid 2>/dev/null
    echo "[`date` $NAME] Killed unstable instance $i after $kill_after seconds"
    sleep 2
    now=$(date +%s)
    i=$((i+1))
  done
}

stable_load &
STABLE_PID=$!

wave_load &
WAVE_PID=$!

unstable_load &
UNSTABLE_PID=$!

echo "[`date` $NAME] Stress test started:"
echo "[`date` $NAME] Stable load    PID: $STABLE_PID"
echo "[`date` $NAME] Wave load      PID: $WAVE_PID"
echo "[`date` $NAME] Unstable load  PID: $UNSTABLE_PID"
echo

wait $UNSTABLE_PID || {
    echo "[`date` $NAME] Unstable load failed"
    find "$LOG_DIR" -type f -exec sh -c 'echo "##### {} #####"; cat "{}"; echo' \;
    exit 1
}
echo "[`date` $NAME] unstable load finished"

wait $WAVE_PID || {
    echo "[`date` $NAME] Wave load failed"
    find "$LOG_DIR" -type f -exec sh -c 'echo "##### {} #####"; cat "{}"; echo' \;
    exit 1
}
echo "[`date` $NAME] wave load finished"

wait $STABLE_PID || {
    echo "[`date` $NAME] Stable load failed"
    find "$LOG_DIR" -type f -exec sh -c 'echo "##### {} #####"; cat "{}"; echo' \;
    exit 1
}
echo "[`date` $NAME] stable load finished"

echo "[`date` $NAME] Stress test completed."
