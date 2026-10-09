#!/usr/bin/env bash

# ALTER SYSTEM tests.
#
# ALTER SYSTEM writes global options to <config>.autoconf, RELOAD applies
# them. Old values are commented out, never removed. The config is copied
# to a fresh directory, because ALTER SYSTEM writes next to it.
#
# 1. SET writes the option to the autoconf file but does not apply it.
# 2. RELOAD applies it.
# 3. A second SET comments out the old value and appends the new one.
# 4. An unknown option or a bad value is rejected, the file stays intact.
# 5. RESET ALL comments out everything, the main config value comes back.
# 6. A parameter RELOAD does not apply gets a NOTICE and waits for restart.
# 7. Odyssey restarts with the resulting autoconf file and applies it.

set -uex

WORK=$(mktemp -d)
CONF=$WORK/odyssey.conf
AUTOCONF=$CONF.autoconf
cp /tests/alter-system/alter-system.conf "$CONF"

console() {
	psql -h 127.0.0.1 -p 6432 -U console -d console \
		--quiet --no-align --tuples-only -F '|' -c "$1"
}

fail() {
	echo "$1" >&2
	cat "$AUTOCONF" >&2 || true
	cat /var/log/odyssey.log >&2 || true
	exit 1
}

expect_value() {
	local got
	got=$(console "show config $1" | cut -d'|' -f2)
	[ "$got" = "$2" ] || fail "expected $1 '$2', got '$got'"
}

expect_line() {
	grep -qxF "$1" "$AUTOCONF" || fail "expected line '$1' in $AUTOCONF"
}

expect_error() {
	if console "$1" >/tmp/alter_system.out 2>&1; then
		fail "expected '$1' to fail"
	fi
	grep -qF "$2" /tmp/alter_system.out || {
		cat /tmp/alter_system.out >&2
		fail "expected '$2' in the error of '$1'"
	}
}

/usr/bin/odyssey "$CONF"
sleep 1

expect_value log_debug no

# 1. SET writes the file, the value is not applied yet
console 'alter system set log_debug = yes'
[ -f "$AUTOCONF" ] || fail "ALTER SYSTEM did not create $AUTOCONF"
expect_line 'log_debug yes'
expect_value log_debug no

# 2. RELOAD applies it
console 'reload'
expect_value log_debug yes

# 3. the old value is kept as a comment
console 'alter system set log_debug = no'
console 'reload'
expect_line '# log_debug yes'
expect_line 'log_debug no'
expect_value log_debug no

# 4. rejected changes leave the file intact
before=$(md5sum <"$AUTOCONF")
expect_error 'alter system set nope = 1' \
	'unrecognized configuration parameter "nope"'
expect_error 'alter system set workers = abc' \
	'invalid value for parameter "workers"'
expect_error 'alter system reset nope' \
	'unrecognized configuration parameter "nope"'
expect_error 'alter system set graceful_die_on_errors = yes' \
	'is deprecated and has no effect'
# parses, but RELOAD would reject it: odyssey would not start with it
expect_error 'alter system set coroutine_stack_size = 1' \
	'the configuration with this change is invalid'
[ "$(md5sum <"$AUTOCONF")" = "$before" ] ||
	fail "rejected ALTER SYSTEM changed $AUTOCONF"

# 5. RESET ALL brings back the value of the main config
console 'alter system set log_debug = yes'
console 'reload'
expect_value log_debug yes
console 'alter system reset all'
console 'reload'
[ -z "$(grep -v '^#' "$AUTOCONF")" ] ||
	fail "RESET ALL left active lines in $AUTOCONF"
expect_value log_debug no

# 6. workers needs a restart: NOTICE, saved, not applied by RELOAD
console 'alter system reset log_debug' 2>/tmp/alter_system.notice
if grep -q 'NOTICE' /tmp/alter_system.notice; then
	cat /tmp/alter_system.notice >&2
	fail "unexpected NOTICE for reloadable log_debug"
fi
console 'alter system set workers = auto' 2>/dev/null
expect_line 'workers "auto"'
console 'alter system set workers = 4' 2>/tmp/alter_system.notice
grep -qF 'parameter "workers" cannot be changed without restarting odyssey' \
	/tmp/alter_system.notice || {
	cat /tmp/alter_system.notice >&2
	fail "expected a restart NOTICE for workers"
}
expect_line 'workers 4'
console 'reload'
expect_value workers 2

ody-stop

# 7. the autoconf file with history parses on start and takes effect
/usr/bin/odyssey "$CONF"
sleep 1

expect_value log_debug no
expect_value workers 4
[ ! -e "$AUTOCONF.tmp" ] || fail "temporary file $AUTOCONF.tmp left behind"

ody-stop

rm -rf "$WORK"
