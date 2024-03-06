#!/bin/bash -x

set -ex

/usr/bin/odyssey /copy/config.conf

(for i in {1..1000}; do 
echo "run_id_${i},task_id_${i},Some random ${i}th text"; 
done | psql postgresql://postgres@localhost:6432/db -c "COPY copy_test FROM STDIN (FORMAT csv);";) > /dev/null 2>&1 || {
    echo "ERROR: COPY BUG"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

ody-stop
