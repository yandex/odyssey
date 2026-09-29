#!/bin/bash

# SHOW DATABASES merges reload generations of a rule, keeping their clients
# in the total. Independent rules sharing a backend database stay separate.

set -euo pipefail

CONF=/tmp/show-databases.conf
cp /tests/show-databases/conf.conf "$CONF"

/usr/bin/odyssey "$CONF"
sleep 1

sleep1=
sleep2=
sleep_app=

cleanup() {
	local rc=$?
	if [ -n "${sleep1:-}" ]; then
		kill "$sleep1" 2>/dev/null || true
		wait "$sleep1" 2>/dev/null || true
	fi
	if [ -n "${sleep2:-}" ]; then
		kill "$sleep2" 2>/dev/null || true
		wait "$sleep2" 2>/dev/null || true
	fi
	if [ -n "${sleep_app:-}" ]; then
		kill "$sleep_app" 2>/dev/null || true
		wait "$sleep_app" 2>/dev/null || true
	fi
	ody-stop || rc=1
	exit "$rc"
}
trap cleanup EXIT

console() {
	psql -h 127.0.0.1 -p 6432 -U console -d console \
		-v ON_ERROR_STOP=1 --quiet --no-align --tuples-only -F '|' -c "$1"
}

show_databases() {
	console 'show databases'
}

fail_with_output() {
	echo "$1" >&2
	echo "SHOW DATABASES output:" >&2
	show_databases >&2 || true
	echo "SHOW STORAGES output:" >&2
	console 'show storages' >&2 || true
	echo "SHOW INSTANCE output:" >&2
	console 'show instance' >&2 || true
	cat /var/log/odyssey.log >&2 || true
	exit 1
}

assert_unique_force_user() {
	local out
	out=$(show_databases)
	if [ -z "$out" ]; then
		fail_with_output "show databases returned no rows"
	fi
	printf '%s\n' "$out" | awk -F '|' '
		NF {
			key = $1 SUBSEP $5
			if (seen[key]++) {
				print "duplicate SHOW DATABASES row: " $0 > "/dev/stderr"
				err = 1
			}
		}
		END { exit err+0 }
	' || fail_with_output "duplicate (name, force_user) in SHOW DATABASES"
}

row_for() {
	local name=$1
	local force_user=$2
	show_databases | awk -F '|' -v name="$name" -v force_user="$force_user" '
		$1 == name && $5 == force_user { print; n++ }
		END { if (n != 1) exit 1 }
	'
}

connections_for() {
	local name=$1
	local force_user=$2
	show_databases | awk -F '|' -v name="$name" -v force_user="$force_user" '
		$1 == name && $5 == force_user { s += $10 }
		END { print s+0 }
	'
}

wait_connections() {
	local name=$1
	local force_user=$2
	local want=$3
	local i got
	got=0
	for i in $(seq 1 25); do
		got=$(connections_for "$name" "$force_user")
		if [ "$got" -eq "$want" ]; then
			return 0
		fi
		sleep 0.2
	done
	fail_with_output "$name/$force_user current_connections=$got, expected $want"
}

assert_postgres_row() {
	local host=$1
	local pool_size=$2
	local connections=$3
	local row
	row=$(row_for postgres postgres) || \
		fail_with_output "expected exactly one postgres/postgres row"
	printf '%s\n' "$row" | awk -F '|' -v host="$host" \
		-v pool_size="$pool_size" -v connections="$connections" '
		$2 != host {
			print "postgres/postgres host=" $2 ", expected " host > "/dev/stderr"
			exit 1
		}
		$6 != pool_size {
			print "postgres/postgres pool_size=" $6 ", expected " pool_size > "/dev/stderr"
			exit 1
		}
		$10 != connections {
			print "postgres/postgres current_connections=" $10 ", expected " connections > "/dev/stderr"
			exit 1
		}
	' || fail_with_output "postgres/postgres row mismatch"
}

reload_config() {
	console 'reload' >/dev/null
	local failed
	failed=$(console 'show instance' | awk -F '|' '$1 == "config_load_failed" { print $2 }')
	if [ "$failed" != "0" ]; then
		fail_with_output "reload left config_load_failed=$failed"
	fi
}

storage_host() {
	console 'show storages' | awk -F '|' '
		$1 ~ /^remote/ { print $2; exit }
	'
}

start_sleep() {
	local user=$1
	local out=$2
	psql -h 127.0.0.1 -p 6432 -U "$user" -d postgres \
		-v ON_ERROR_STOP=1 --quiet -c 'SELECT pg_sleep(120)' \
		>"$out" 2>&1 &
}

assert_unique_force_user

start_sleep postgres /tmp/show-databases-sleep1.out
sleep1=$!
wait_connections postgres postgres 1

sed -i 's/pool_ttl 60/pool_ttl 61/' "$CONF"
reload_config

start_sleep postgres /tmp/show-databases-sleep2.out
sleep2=$!
wait_connections postgres postgres 2
assert_unique_force_user
assert_postgres_row 127.0.0.1 10 2

kill "$sleep2" 2>/dev/null || true
wait "$sleep2" 2>/dev/null || true
sleep2=
wait_connections postgres postgres 1

sed -i '/storage "postgres_server"/,/^}/ s/host "127.0.0.1"/host "ip4-localhost"/' "$CONF"
sed -i '/user "postgres" {/,/^	}/ s/pool_size 10/pool_size 20/' "$CONF"
reload_config
got_host=$(storage_host)
if [ "$got_host" != "ip4-localhost" ]; then
	fail_with_output "storage host=$got_host, expected ip4-localhost after reload"
fi

start_sleep postgres /tmp/show-databases-sleep2.out
sleep2=$!
wait_connections postgres postgres 2
assert_unique_force_user
assert_postgres_row ip4-localhost 20 2

start_sleep app /tmp/show-databases-sleep-app.out
sleep_app=$!
wait_connections postgres app 1
wait_connections postgres postgres 2
assert_unique_force_user
assert_postgres_row ip4-localhost 20 2
row_for postgres app >/dev/null || \
	fail_with_output "expected a separate postgres/app row"

psql -h 127.0.0.1 -p 6432 -U postgres -d alias \
	-v ON_ERROR_STOP=1 --quiet -c 'SELECT 1' >/dev/null
wait_connections postgres postgres 2
show_databases | awk -F '|' '
	$1 == "postgres" && $5 == "postgres" {
		if ($4 == "postgres" && $6 == 20 && $8 == "session" && $10 == 2)
			original++
		else if ($4 == "alias" && $6 == 7 && $8 == "transaction" && $10 == 0)
			alias++
		else
			exit 1
	}
	END { if (original != 1 || alias != 1) exit 1 }
' || fail_with_output "independent rules sharing storage_db were merged"
