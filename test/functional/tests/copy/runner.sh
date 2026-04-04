#!/bin/bash -x

set -eux

csv_file=$(mktemp)
for i in {1..1000}; do
	echo "run_id_${i},task_id_${i},Some random ${i}th text" >> $csv_file
done
sort $csv_file -o $csv_file

PSQL="psql postgresql://postgres@localhost:6432/db"

get_server_ptr() {
	result=$(psql postgresql://console@localhost:6432/console -t -c "show servers;" | grep -Eo 's[0-9a-f]{12}')
	count=$(echo "$result" | wc -l)
	if [ "$count" -ne 1 ]; then
		echo "ERROR: expected 1 server connection, got $count" >&2
		exit 1
	fi
	echo "$result"
}

test_copy_in() {
	$PSQL -c "TRUNCATE copy_test;" || return 1

	cat $csv_file | $PSQL -c "COPY copy_test FROM STDIN (FORMAT csv);" || return 1

	count=$($PSQL -t -c "SELECT COUNT(*) FROM copy_test;" | tr -d ' ')
	if [ "$count" -ne 1000 ]; then
		echo "ERROR: expected 1000 rows, got $count"
		return 1
	fi

	for i in $(seq 1 50 1000); do
		row=$($PSQL -t -c "SELECT description FROM copy_test WHERE run_id = 'run_id_${i}';" | tr -d ' ')
		if [ "$row" != "Somerandom${i}thtext" ]; then
			echo "ERROR: unexpected row content: $row"
			return 1
		fi
	done

	return 0
}

test_copy_out() {
	tmpfile=$(mktemp)
	$PSQL -c "COPY copy_test TO STDOUT (FORMAT csv);" > "$tmpfile" || { rm "$tmpfile"; return 1; }

	line_count=$(wc -l < "$tmpfile")
	if [ "$line_count" -ne 1000 ]; then
		echo "ERROR: copy out: expected 1000 lines, got $line_count"
		rm "$tmpfile"
		return 1
	fi

	sort "$tmpfile" -o "$tmpfile"

	diff $csv_file $tmpfile || {
		echo "ERROR: copy out data mismatch"
		return 1
	}

	rm $tmpfile

	return 0
}

test_copy_in_error() {
	$PSQL -c "select 42"

	ptr_before=$(get_server_ptr)
	if [ -z "$ptr_before" ]; then
		echo "ERROR: no server connection before copy error test"
		return 1
	fi

	echo "bad,data,with,too,many,columns,here" | \
		$PSQL -c "COPY copy_test FROM STDIN (FORMAT csv);" 2>&1 && {
		echo "ERROR: expected error on bad data, but got success"
		return 1
	}

	ptr_after=$(get_server_ptr)
	if [ "$ptr_before" != "$ptr_after" ]; then
		echo "ERROR: server connection changed after copy error: $ptr_before -> $ptr_after"
		return 1
	fi

	return 0
}

run_tests() {
	local result=0

	test_copy_in || { echo "FAIL: copy in"; result=1; }
	test_copy_out || { echo "FAIL: copy out"; result=1; }
	test_copy_in_error || { echo "FAIL: copy in error handling"; result=1; }

	return $result
}

/usr/bin/odyssey /tests/copy/config.conf
sleep 1

$PSQL -c "
	DROP TABLE IF EXISTS copy_test;
	CREATE TABLE copy_test (
		run_id TEXT,
		task_id TEXT,
		description TEXT
	);
"

with_pstmts_ok=0
run_tests && with_pstmts_ok=1
ody-stop

echo "" > /var/log/odyssey.log

/usr/bin/odyssey /tests/copy/config_without_pstmt.conf
sleep 1
without_pstmts_ok=0
run_tests && without_pstmts_ok=1
ody-stop

dump_logs() {
    cat /var/log/odyssey.log
    echo ""
    cat /var/log/postgresql/postgresql-16-main.log
}

if [ $with_pstmts_ok -eq 0 -a $without_pstmts_ok -eq 1 ]; then
    echo "ERROR: copy bug specific to pool_reserve_prepared_statement=yes"
    dump_logs
    exit 1
fi

if [ $with_pstmts_ok -eq 0 -o $without_pstmts_ok -eq 0 ]; then
    echo "ERROR: copy bug"
    dump_logs
    exit 1
fi

echo "OK: all copy tests passed"
exit 0