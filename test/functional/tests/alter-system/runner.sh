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
# 8. Concurrent changes are all preserved in the autoconf history.
# 9. Oversized files and write errors leave the autoconf file intact.

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
[ "$(stat -c '%a' "$AUTOCONF")" = 600 ] || fail "expected autoconf permissions 0600"
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

# Duration strings preserve units and are applied only on reload.
expect_value stats_interval 3
console "alter system set stats_interval = '10s'"
expect_line 'stats_interval "10s"'
expect_value stats_interval 3
console 'reload'
expect_value stats_interval 10
console "alter system set stats_interval = '1.5 min'"
console 'reload'
expect_value stats_interval 90
console "alter system set stats_interval = '10'"
console 'reload'
expect_value stats_interval 10
console "alter system set backend_connect_timeout_ms = '10'"
console 'reload'
expect_value backend_connect_timeout_ms 10
console "alter system set backend_connect_timeout_ms = '10s'"
console 'reload'
expect_value backend_connect_timeout_ms 10000
console "alter system set cancel_queue_timeout_ms = '-1'" 2>/dev/null
expect_line 'cancel_queue_timeout_ms "-1"'

# 4. rejected changes leave the file intact
before=$(md5sum <"$AUTOCONF")
expect_error "alter system set stats_interval = 'oops'" \
	'invalid value for parameter "stats_interval"'
expect_error "alter system set stats_interval = '1h30m'" \
	'invalid value for parameter "stats_interval"'
expect_error "alter system set stats_interval = '10S'" \
	'invalid value for parameter "stats_interval"'
expect_error "alter system set cancel_queue_timeout_ms = '-9223372036854775809'" \
	'invalid value for parameter "cancel_queue_timeout_ms"'
expect_error "alter system set stats_interval = '-1s'" \
	'must be between'
expect_error "alter system set stats_interval = '2147483648ms'" \
	'must be between'
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
# A valid SET must also reject an invalid storage in the main config.
cp "$CONF" "$CONF.saved"
sed '/storage "postgres_server"/,/^}/s/host "127.0.0.1"/host "["/' \
	"$CONF.saved" >"$CONF"
expect_error 'alter system set log_debug = yes' \
	'the configuration with this change is invalid'
mv "$CONF.saved" "$CONF"
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

# Quoted values must retain their units after restarting from autoconf.
console "alter system set stats_interval = '10s'"
console "alter system set backend_connect_timeout_ms = '10'"

ody-stop

# 7. the autoconf file with history parses on start and takes effect
/usr/bin/odyssey "$CONF"
sleep 1

expect_value log_debug no
expect_value workers 4
expect_value stats_interval 10
expect_value backend_connect_timeout_ms 10
[ ! -e "$AUTOCONF.tmp" ] || fail "temporary file $AUTOCONF.tmp left behind"

# 8. Concurrent writers must not overwrite each other's changes.
pids=()
for i in $(seq 1 12); do
	console "alter system set availability_zone = 'parallel_$i'" &
	pids+=("$!")
done
for pid in "${pids[@]}"; do
	wait "$pid" || fail "concurrent ALTER SYSTEM failed"
done
for i in $(seq 1 12); do
	grep -Eq "^(# )?availability_zone \"parallel_$i\"$" "$AUTOCONF" ||
		fail "concurrent ALTER SYSTEM lost parallel_$i"
done
[ "$(grep -c '^availability_zone ' "$AUTOCONF")" -eq 1 ] ||
	fail "concurrent ALTER SYSTEM left multiple active values"
[ ! -e "$AUTOCONF.tmp" ] || fail "temporary file $AUTOCONF.tmp left behind"
console 'reload'
expect_value availability_zone a

# 9. Both the existing file and the candidate are limited to 1 MiB.
cp "$AUTOCONF" "$AUTOCONF.saved"
limit=$((1024 * 1024))
head -c "$((limit + 1))" /dev/zero | tr '\0' '#' >"$AUTOCONF"
before=$(md5sum <"$AUTOCONF")
expect_error 'alter system set log_debug = yes' 'is too large'
[ "$(md5sum <"$AUTOCONF")" = "$before" ] || fail "oversized autoconf was changed"

head -c "$((limit - 2))" /dev/zero | tr '\0' '#' >"$AUTOCONF"
printf '\n' >>"$AUTOCONF"
before=$(md5sum <"$AUTOCONF")
expect_error 'alter system set log_debug = yes' 'would exceed'
[ "$(md5sum <"$AUTOCONF")" = "$before" ] || fail "autoconf grew past the limit"
[ ! -e "$AUTOCONF.tmp" ] || fail "temporary file $AUTOCONF.tmp left behind"
mv "$AUTOCONF.saved" "$AUTOCONF"

# The config directory may disappear after startup. A write must fail cleanly.
before=$(md5sum <"$AUTOCONF")
mv "$WORK" "$WORK.saved"
expect_error 'alter system set log_debug = yes' 'could not create'
mv "$WORK.saved" "$WORK"
[ "$(md5sum <"$AUTOCONF")" = "$before" ] || fail "failed write changed autoconf"
[ ! -e "$AUTOCONF.tmp" ] || fail "temporary file $AUTOCONF.tmp left behind"

# A later command still succeeds after the failed writes.
console 'alter system set log_debug = yes'
expect_line 'log_debug yes'

ody-stop

rm -rf "$WORK"
