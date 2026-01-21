#!/bin/bash -x

set -ex

test_successful() {
	(for i in {1..1000}; do 
		echo "run_id_${i},task_id_${i},Some random ${i}th text"; 
	done | psql postgresql://postgres@localhost:6432/db -c "COPY copy_test FROM STDIN (FORMAT csv);";) > /dev/null || { 
			echo 1
			return
		}
	echo 0
}

/usr/bin/odyssey /tests/copy/config.conf
sleep 1
with_pstmts_test_successful=$(test_successful)
ody-stop

echo "" > /var/log/odyssey.log

/usr/bin/odyssey /tests/copy/config_without_pstmt.conf
sleep 1
without_pstmts_test_successful=$(test_successful)
ody-stop

if [ $with_pstmts_test_successful -eq 1 -a $without_pstmts_test_successful -eq 0 ]; then {
    echo "ERROR: copy bug when pool_reserve_prepared_statement setting to yes"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
} fi

if [ $with_pstmts_test_successful -eq 1 -o $without_pstmts_test_successful -eq 1 ]; then {
	echo "ERROR: copy bug"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
} fi
