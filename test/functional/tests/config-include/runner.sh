#!/usr/bin/env bash

# Config file inclusion.
#
# Including two config files that both set common options
# is duplicate and leads to an error.

set -uex

CONF=/tests/config-include/conf.conf
DUP_CONF=/tests/config-include/dup.conf

/usr/bin/odyssey "$CONF"
sleep 1

psql -h 127.0.0.1 -p 6432 -U console -d console \
	--quiet --no-align --tuples-only -F '|' -c 'show config' |
	grep -E '^availability_zone\|' | grep -q '^availability_zone|a' || {
	echo "expected availability_zone 'a' from included config" >&2
	cat /var/log/odyssey.log >&2 || true
	exit 1
}

ody-stop

# reject dups
if /usr/bin/odyssey "$DUP_CONF" >/tmp/dup.out 2>&1; then
	echo "duplicate availability_zone must fail to start" >&2
	cat /tmp/dup.out >&2 || true
	exit 1
fi

grep -q "duplicate option 'availability_zone'" /tmp/dup.out || {
	echo "expected 'duplicate option availability_zone' in startup output" >&2
	cat /tmp/dup.out >&2 || true
	exit 1
}
