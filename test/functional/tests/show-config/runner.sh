#!/bin/bash

# SHOW CONFIG reports the configuration of the running process, not the
# file on disk. RELOAD applies only part of the global configuration, so
# editing odyssey.conf and reloading is not enough to tell what is in
# effect - that is what this test checks.

set -uex

CONF=/tmp/show-config.conf
cp /tests/show-config/conf.conf "$CONF"

/usr/bin/odyssey "$CONF"
sleep 1

console() {
	psql -h 127.0.0.1 -p 6432 -U console -d console \
		--quiet --no-align --tuples-only -F '|' -c "$1"
}

value_of() {
	console 'show config' | awk -F '|' -v key="$1" '$1 == key { print $2 }'
}

changeable_of() {
	console 'show config' | awk -F '|' -v key="$1" '$1 == key { print $4 }'
}

expect() {
	local what="$1" got="$2" want="$3"
	if [ "$got" != "$want" ]; then
		echo "$what: got '$got', expected '$want'" >&2
		cat /var/log/odyssey.log >&2 || true
		exit 1
	fi
}

# the output is not empty
test "$(console 'show config' | grep -c '|')" -gt 0 || {
	echo "show config returned no rows" >&2
	exit 1
}

# every row carries all four columns
console 'show config' | awk -F '|' 'NF != 4 {
	print "unexpected column count: " $0 > "/dev/stderr"; exit 1
}'

# changeable is a yes/no flag
console 'show config' | awk -F '|' '$4 != "yes" && $4 != "no" {
	print "unexpected changeable value: " $0 > "/dev/stderr"; exit 1
}'

# no value is left empty
console 'show config' | awk -F '|' '$2 == "" || $3 == "" {
	print "empty value or default: " $0 > "/dev/stderr"; exit 1
}'

# starting point, taken from conf.conf
expect "log_debug value" "$(value_of log_debug)" "no"
expect "log_debug changeable" "$(changeable_of log_debug)" "yes"
expect "workers value" "$(value_of workers)" "2"
expect "workers changeable" "$(changeable_of workers)" "no"

# the file changes, RELOAD is not issued: the output must not follow
sed -i 's/^log_debug no$/log_debug yes/' "$CONF"
expect "log_debug before reload" "$(value_of log_debug)" "no"

# RELOAD applies log_debug
psql -h 127.0.0.1 -p 6432 -U console -d console -c 'reload'
sleep 1
expect "log_debug after reload" "$(value_of log_debug)" "yes"

# RELOAD does not apply workers: the process keeps the old value and the
# output has to show that
sed -i 's/^workers 2$/workers 8/' "$CONF"
psql -h 127.0.0.1 -p 6432 -U console -d console -c 'reload'
sleep 1
expect "workers after reload" "$(value_of workers)" "2"
expect "workers changeable after reload" "$(changeable_of workers)" "no"

# defaults come from od_config_init and do not depend on the file
expect "workers default" \
	"$(console 'show config' | awk -F '|' '$1 == "workers" { print $3 }')" "1"

ody-stop
