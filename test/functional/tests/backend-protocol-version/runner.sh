#!/bin/bash

set -euo pipefail

# PostgreSQL 16 is installed in the test image and only speaks protocol 3.0.
pg_bin=/usr/lib/postgresql/16/bin
work_dir=$(sudo -u postgres mktemp -d)

cleanup() {
	ody-stop || true
	if [ -f "$work_dir/pgdata/postmaster.pid" ]; then
		sudo -u postgres "$pg_bin/pg_ctl" -D "$work_dir/pgdata" -m fast -w stop
	fi
	rm -rf "$work_dir"
}
trap cleanup EXIT

chmod 755 "$work_dir"
sudo -u postgres "$pg_bin/initdb" -D "$work_dir/pgdata" -U postgres -A trust --no-locale
sudo -u postgres "$pg_bin/pg_ctl" -D "$work_dir/pgdata" -l "$work_dir/pgdata/server.log" \
	-o "-h 127.0.0.1 -p 55432 -k $work_dir" -w start
pgbench 'host=127.0.0.1 port=55432 user=postgres dbname=postgres' -i

# Reuse the cancel workload with a 3.2 frontend and a backend negotiating 3.0.
sed -e 's/\[127.0.0.1\]:5432/[127.0.0.1]:55432/' \
	-e 's/log_debug no/log_debug yes/' \
	-e "s|/var/log/odyssey.log|$work_dir/odyssey.log|" \
	/tests/cancel/cancel.conf > "$work_dir/odyssey.conf"
bash /tests/cancel-3.2/runner.sh "$work_dir/odyssey.conf"
grep -q 'server negotiated protocol 3.0' "$work_dir/odyssey.log"
