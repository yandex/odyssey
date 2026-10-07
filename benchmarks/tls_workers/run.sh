#!/bin/bash
#
# Impact of client TLS handshakes on persistent clients, with and without
# tls_workers.
#
# usage: run.sh <odyssey binary> <workers> <tls_workers> reconnect
#        run.sh <odyssey binary> <workers> <tls_workers> burst <tls|plain>
#
# reconnect: 16 persistent clients without TLS run "select 67;", their tps is
#   measured alone (A), next to 32 clients reconnecting on every transaction
#   without TLS (B) and with TLS (C).
#
# burst: the same persistent clients, 5 times 100 clients connect at once
#   (burst.py), run one query and disconnect. Persistent clients latency is
#   taken from the pgbench per-transaction log inside the burst windows.
#
# Postgres and pgbench run in docker (postgres:16), the burst client runs
# on the host. Configure the cpu layout so that odyssey, postgres and the
# burst client do not share cores.

set -e

ODYSSEY=$(realpath "$1")
WORKERS=$2
TLS_WORKERS=$3
MODE=$4
BURST_MODE=$5

ODY_CPUS=${ODY_CPUS:-0-3}
PG_CPUS=${PG_CPUS:-4-6}
CLIENT_CPU=${CLIENT_CPU:-7}
DUR=${DUR:-20}
PORT=${PORT:-16432}
PG_PORT=${PG_PORT:-15432}
WORK=${WORK:-$(pwd)/out}
PG=odyssey-bench-pg

DIR=$(dirname "$(realpath "$0")")
mkdir -p "$WORK"

if [ ! -f "$WORK/server.key" ]; then
	openssl req -x509 -newkey rsa:2048 -nodes -days 30 -subj /CN=localhost \
		-keyout "$WORK/server.key" -out "$WORK/server.pem" 2>/dev/null
fi

if ! docker ps --format '{{.Names}}' | grep -qx $PG; then
	docker rm -f $PG >/dev/null 2>&1 || true
	docker run -d --name $PG --network host --cpuset-cpus "$PG_CPUS" \
		-e POSTGRES_HOST_AUTH_METHOD=trust -e PGPORT=$PG_PORT \
		postgres:16 -c max_connections=500 -c shared_buffers=256MB >/dev/null
	until docker exec $PG pg_isready -q -h 127.0.0.1 -p $PG_PORT; do
		sleep 0.5
	done
	docker exec $PG sh -c 'echo "select 67;" > /tmp/q.sql'
fi

cat > "$WORK/odyssey.conf" <<EOF
daemonize no
log_to_stdout no
log_file "$WORK/odyssey.log"
log_format "%p %t %l [%i %s] (%c) %m\n"
log_session no
log_config no
log_stats no
locks_dir "$WORK"
unix_socket_dir "$WORK"
unix_socket_mode "0644"
workers $WORKERS
tls_workers $TLS_WORKERS

listen {
	host "127.0.0.1"
	port $PORT
	tls "allow"
	tls_key_file "$WORK/server.key"
	tls_cert_file "$WORK/server.pem"
}

storage "pg" {
	type "remote"
	host "127.0.0.1"
	port $PG_PORT
}

database default {
	user default {
		authentication "none"
		storage "pg"
		pool "session"
		pool_size 200
	}
}
EOF

taskset -c "$ODY_CPUS" "$ODYSSEY" "$WORK/odyssey.conf" &
OD=$!
trap 'kill $OD 2>/dev/null; wait $OD 2>/dev/null' EXIT
sleep 1

CONN="host=127.0.0.1 port=$PORT user=postgres dbname=postgres"
LABEL="workers=$WORKERS tls_workers=$TLS_WORKERS"

pgb() {
	docker exec $PG pgbench "$@" -n -f /tmp/q.sql 2>&1
}

num() {
	grep -E "^$1" | head -1 | sed -E 's/[^=]*= *([0-9.]+).*/\1/'
}

# warmup, also fills the server pool
pgb "$CONN sslmode=disable" -c 16 -j 4 -T 3 >/dev/null
taskset -c "$CLIENT_CPU" python3 -I "$DIR/burst.py" disable 100 $PORT >/dev/null

case $MODE in
reconnect)
	r=$(pgb "$CONN sslmode=disable" -c 16 -j 4 -T $DUR)
	echo "$LABEL A: tps $(echo "$r" | num tps), latency $(echo "$r" | num 'latency average') ms"
	for ph in B C; do
		ssl=disable
		[ $ph = C ] && ssl=require
		pgb "$CONN sslmode=$ssl" -c 32 -j 4 -C -T $((DUR + 2)) > "$WORK/storm.out" &
		ST=$!
		sleep 1
		r=$(pgb "$CONN sslmode=disable" -c 16 -j 4 -T $DUR)
		wait $ST
		echo "$LABEL $ph: tps $(echo "$r" | num tps), latency $(echo "$r" | num 'latency average') ms," \
			"reconnects/s $(num tps < "$WORK/storm.out")"
	done
	;;
burst)
	ssl=require
	[ "$BURST_MODE" = plain ] && ssl=disable
	R="$WORK/burst-$WORKERS-$TLS_WORKERS-$BURST_MODE"
	rm -rf "$R"
	mkdir -p "$R"
	docker exec $PG sh -c 'rm -rf /tmp/steady && mkdir -p /tmp/steady'
	pgb "$CONN sslmode=disable" -c 16 -j 4 -T 16 -l --log-prefix=/tmp/steady/s > "$R/steady.out" &
	ST=$!
	sleep 3
	for i in 1 2 3 4 5; do
		taskset -c "$CLIENT_CPU" python3 -I "$DIR/burst.py" $ssl 100 $PORT >> "$R/bursts.txt"
		sleep 2
	done
	wait $ST
	docker cp $PG:/tmp/steady "$R/" >/dev/null
	echo "$LABEL $BURST_MODE burst: $(python3 -I "$DIR/stats.py" "$R")"
	;;
*)
	echo "unknown mode $MODE"
	exit 1
	;;
esac
