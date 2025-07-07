#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/min_pool_size/config.conf
sleep 1

# create some connections for od_frontend_setup_params
pgbench 'host=localhost port=6432 user=postgres dbname=postgres' --select-only --progress 1 --no-vacuum -T 5 -j 1 -c 5

psql 'host=localhost port=6432 user=console dbname=console' -t -c 'show servers' | sed '/^$/d' | sort > /tmp/setup-connections

# wait for this connections to expire by ttl and new connections created
sleep 11

psql 'host=localhost port=6432 user=console dbname=console' -t -c 'show servers' | sed '/^$/d' | sort > /tmp/newly-preallocated

if diff /tmp/setup-connections /tmp/newly-preallocated; then
	echo "setup connections must be differ from newly preallocated"
	cat /tmp/setup-connections
	cat /tmp/newly-preallocated
	exit 1
fi

servers_count=`cat /tmp/newly-preallocated | wc -l`
if [ $servers_count != 5 ]; then
	echo 'there must be 5 newly preallocated servers'
	cat /var/log/odyssey.log
	exit 1
fi

# use this connections
pgbench 'host=localhost port=6432 user=postgres dbname=postgres' --select-only --progress 1 --no-vacuum -T 5 -j 1 -c 5

# make sure that no new connections was created
psql 'host=localhost port=6432 user=console dbname=console' -t -c 'show servers' | sed '/^$/d' | sort > /tmp/post-usage

diff /tmp/newly-preallocated /tmp/post-usage || {
	echo "server lists are different"
	cat /tmp/newly-preallocated
	cat /tmp/post-usage
	cat /var/log/odyssey.log
	exit 1
}

ody-stop
