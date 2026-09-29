#!/usr/bin/env bash

# Reload parses config on the system coroutine (default 32 pages). Nested
# include must keep the process alive for SQL RELOAD and SIGHUP.

set -uexo pipefail

WORKDIR=/tmp/reload-include-stack
CONF=/tmp/reload-include-stack.conf
SRC=/tests/reload-include-stack/conf.conf
LIMIT=16

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
cp "$SRC" "$CONF"

console() {
	PGCONNECT_TIMEOUT=2 psql -X -w -v ON_ERROR_STOP=1 \
		-h 127.0.0.1 -p 6432 -U console -d console \
		--quiet --no-align --tuples-only -F '|' -c "$1"
}

expect() {
	local what="$1" got="$2" want="$3"
	if [ "$got" != "$want" ]; then
		echo "$what: got '$got', expected '$want'" >&2
		cat /var/log/odyssey.log >&2 || true
		exit 1
	fi
}

instance_flag() {
	console 'show instance' | awk -F '|' -v key="$1" '$1 == key { print $2 }'
}

config_value() {
	console 'show config' | awk -F '|' -v key="$1" '$1 == key { print $2 }'
}

wait_config_value() {
	local key="$1" want="$2" got="" deadline=$((SECONDS + 10))
	while [ "$SECONDS" -lt "$deadline" ]; do
		if got=$(config_value "$key") && [ "$got" = "$want" ]; then
			return
		fi
		sleep 0.1
	done
	expect "$key before timeout" "$got" "$want"
}

write_chain() {
	local depth="$1"
	local leaf="$2"
	local i

	{
		echo "include \"$WORKDIR/i1.conf\""
		grep -vE '^(availability_zone|log_debug) ' "$SRC"
	} >"$CONF"

	if [ "$depth" -gt 1 ]; then
		for i in $(seq 1 $((depth - 1))); do
			echo "include \"$WORKDIR/i$((i + 1)).conf\"" >"$WORKDIR/i${i}.conf"
		done
	fi
	printf '%s\n' "$leaf" >"$WORKDIR/i${depth}.conf"
}

alive() {
	console 'show instance' >/dev/null || {
		echo "console is gone after $1" >&2
		cat /var/log/odyssey.log >&2 || true
		exit 1
	}
}

/usr/bin/odyssey "$CONF"
wait_config_value availability_zone start

expect "config_load_failed at start" "$(instance_flag config_load_failed)" "0"

sed -i 's/^log_debug no$/log_debug yes/' "$CONF"
console 'reload' >/dev/null
alive "sql reload without include"
expect "log_debug after sql reload" "$(config_value log_debug)" "yes"
expect "config_load_failed after sql reload" "$(instance_flag config_load_failed)" "0"

sed -i 's/^log_debug yes$/log_debug no/' "$CONF"
kill -s HUP "$(pidof odyssey)"
wait_config_value log_debug no
alive "sighup without include"
expect "config_load_failed after sighup" "$(instance_flag config_load_failed)" "0"

write_chain "$LIMIT" "log_debug yes"
console 'reload' >/dev/null
alive "sql reload include depth $LIMIT"
expect "log_debug after include depth $LIMIT" \
	"$(config_value log_debug)" "yes"
expect "config_load_failed after include depth $LIMIT" \
	"$(instance_flag config_load_failed)" "0"

write_chain 5 "log_debug no"
kill -s HUP "$(pidof odyssey)"
wait_config_value log_debug no
alive "sighup include depth 5"
expect "config_load_failed after sighup include" \
	"$(instance_flag config_load_failed)" "0"

write_chain $((LIMIT + 1)) "log_debug yes"
console 'reload' >/dev/null
alive "sql reload include depth $((LIMIT + 1))"
expect "log_debug after too-deep include" "$(config_value log_debug)" "no"
expect "config_load_failed after too-deep include" \
	"$(instance_flag config_load_failed)" "1"

{
	echo "include \"$WORKDIR/missing.conf\""
	cat "$SRC"
} >"$CONF"
console 'reload' >/dev/null
alive "sql reload missing include"
expect "log_debug after missing include" "$(config_value log_debug)" "no"
expect "config_load_failed after missing include" \
	"$(instance_flag config_load_failed)" "1"

ody-stop
