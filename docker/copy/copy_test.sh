#!/bin/bash -x

set -ex

test_successful() {
	(for i in {1..1000}; do 
		echo "run_id_${i},task_id_${i},Some random ${i}th text"; 
	done | psql postgresql://postgres@localhost:6432/db -c "COPY copy_test FROM STDIN (FORMAT csv);";) > /dev/null 2>&1 || { 
			echo 1
			return
		}
	echo 0
}

/usr/bin/odyssey /copy/config.conf
with_pstmts_test_successful=$(test_successful)
ody-stop

sed -i '/pool_reserve_prepared_statement yes/d' /copy/config.conf

/usr/bin/odyssey /copy/config.conf
without_pstmts_test_successful=$(test_successful)
ody-stop

if [ $with_pstmts_test_successful -eq 1 -a $without_pstmts_test_successful -eq 0 ]; then {
    echo "ERROR: copy bug when pool_reserve_prepared_statement setting to yes"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
} fi

if [ $with_pstmts_test_successful -eq 1 -o $without_pstmts_test_successful -eq 1 ]; then {
	echo "ERROR: copy bug"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
} fi
