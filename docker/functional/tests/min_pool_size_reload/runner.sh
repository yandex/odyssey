#!/bin/bash -x

set -ex

/usr/bin/odyssey /tests/min_pool_size_reload/config.conf
sleep 1

# wait for min_pool size connections to be created
sleep 6

psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'

servers_count=`psql 'host=localhost port=6432 user=console dbname=console' -t -c 'show servers' | sed '/^$/d' | wc -l`
if [ $servers_count != 5 ]; then
	echo 'there must be 5 newly preallocated servers'
	cat /var/log/odyssey.log
	exit 1
fi

# ensure min_pool_size are reloaded correctly
sed -i 's/min_pool_size\ 5/min_pool_size\ 10/g' /tests/min_pool_size_reload/config.conf
kill -sHUP $(pidof odyssey)
sleep 1

# ensure new connections created
sleep 11

psql 'host=localhost port=6432 user=console dbname=console' -c 'show servers'

servers_count=`psql 'host=localhost port=6432 user=console dbname=console' -t -c 'show servers' | sed '/^$/d' | wc -l`
if [ $servers_count != 10 ]; then
	echo "min_pool_size was not reloaded correctly"
	exit 1
fi

ody-stop
