#!/bin/bash -x

set -ex

check_servers_expiration() {
	pgbench 'host=localhost port=6432 user=postgres dbname=postgres' -T 2 -j 1 -c 5 --no-vacuum --progress 1 --select-only

	date -R
	# do not forget remove empty lines by sed
	psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'
	servers_count=`psql 'host=localhost port=6432 user=console dbname=console' -t -c 'show servers' | sed '/^$/d' | wc -l`
	if [ $servers_count != 5 ]; then
		echo 'there must be 5 servers'
		cat /var/log/odyssey.log
		exit 1
	fi

	sleep 2

	date -R
	# lifetime or pool_ttl not expired
	psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'
	servers_count=`psql 'host=localhost port=6432 user=console dbname=console' -t -c 'show servers' | sed '/^$/d' | wc -l`
	if [ $servers_count != 5 ]; then
		echo 'there must be 5 servers'
		cat /var/log/odyssey.log
		exit 1
	fi

	# wait for lifetime or pool_ttl to expire
	# get some higher value than total lifetime, because closing connections
	# gets some time
	sleep 6

	date -R
	psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'
	servers_count=`psql 'host=localhost port=6432 user=console dbname=console' -t -c 'show servers' | sed '/^$/d' | wc -l`
	if [ $servers_count != 0 ]; then
		echo 'there must be 0 servers, because of its lifetime'
		cat /var/log/odyssey.log
		exit 1
	fi
}

/usr/bin/odyssey /tests/server-lifetime/pool_ttl.conf
sleep 1

check_servers_expiration || exit 1

ody-stop

/usr/bin/odyssey /tests/server-lifetime/server_lifetime.conf
sleep 1

check_servers_expiration || exit 1

ody-stop
