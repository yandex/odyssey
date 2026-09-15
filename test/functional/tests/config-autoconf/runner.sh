#!/usr/bin/env bash

# Autoconf tests.
#
# 1. main.conf sets availability_zone "a", while autoconf overrides it to "b".
#    Odyssey should start and report "b" via SHOW CONFIG.
# 2. bad_include.conf.autoconf contains an include directive, which is forbidden
#    in autoconf. Odyssey must refuse to start.

set -uex

CONF=/tests/config-autoconf/main.conf
BAD_CONF=/tests/config-autoconf/bad_include.conf

# 1. Override global option in autoconf
/usr/bin/odyssey "$CONF"
sleep 1

psql -h 127.0.0.1 -p 6432 -U console -d console \
	--quiet --no-align --tuples-only -F '|' -c 'show config' |
	grep -E '^availability_zone\|' | grep -q '^availability_zone|b' || {
	echo "expected availability_zone 'b' overridden by autoconf" >&2
	cat /var/log/odyssey.log >&2 || true
	exit 1
}

ody-stop

# 2. Include is forbidden in autoconf
if /usr/bin/odyssey "$BAD_CONF" >/tmp/bad_include.out 2>&1; then
	echo "autoconf with include must fail to start" >&2
	cat /tmp/bad_include.out >&2 || true
	exit 1
fi

grep -q "includes are forbidden in this context" /tmp/bad_include.out || {
	echo "expected 'includes are forbidden in this context' in startup output" >&2
	cat /tmp/bad_include.out >&2 || true
	exit 1
}
