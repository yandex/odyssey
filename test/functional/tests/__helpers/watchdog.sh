# Controlled heartbeat shared by the lag polling tests. The watchdog still
# queries the real replica, but lag does not depend on elapsed wall-clock time.

watchdog_load() {
	psql -X -v ON_ERROR_STOP=1 -h localhost -p "${watchdog_console_port:-6432}" -U user1 -d console -c "LOAD $1"
}

watchdog_resume() {
	if [ "${watchdog_before_poll_loaded:-false}" = true ]; then
		watchdog_load watchdog_before_poll || true
		watchdog_before_poll_loaded=false
	fi
	if [ "${watchdog_after_poll_loaded:-false}" = true ]; then
		watchdog_load watchdog_after_poll || true
		watchdog_after_poll_loaded=false
	fi
}

watchdog_wait_for_replay() {
	local lsn
	lsn=$(psql -XAt -v ON_ERROR_STOP=1 -h localhost -p 5432 -U postgres -d postgres -c 'SELECT pg_current_wal_lsn()')
	timeout 30 bash -c '
		until [ "$(psql -XAt -v ON_ERROR_STOP=1 -h localhost -p 5433 -U postgres -d postgres -c "SELECT pg_last_wal_replay_lsn() >= '\''$1'\''::pg_lsn")" = t ]; do
			sleep 0.1
		done
	' _ "$lsn"
}

watchdog_prepare() {
	watchdog_database=${1:-postgres}
	psql -X -v ON_ERROR_STOP=1 -h localhost -p 5432 -U postgres -d "$watchdog_database" -c '
		DROP TABLE IF EXISTS watchdog_lag;
		CREATE TABLE watchdog_lag (lag_sec integer NOT NULL);
		INSERT INTO watchdog_lag VALUES (0);
	'
	watchdog_wait_for_replay
	trap watchdog_resume EXIT
}

watchdog_wait_for_point() {
	if ! timeout 10 bash -c '
		until grep -qF "injection $1: parking" /var/log/odyssey.log; do
			sleep 0.1
		done
	' _ "$1"; then
		echo "watchdog did not reach injection point $1"
		cat /var/log/odyssey.log
		return 1
	fi
}

watchdog_pause() {
	watchdog_load watchdog_before_poll
	watchdog_before_poll_loaded=true
	watchdog_wait_for_point watchdog_before_poll
}

watchdog_set_lag() {
	# The previous poll has finished. Make the new heartbeat visible on the
	# replica before allowing the next poll, then wait until it is cached.
	psql -X -v ON_ERROR_STOP=1 -h localhost -p 5432 -U postgres -d "$watchdog_database" -c "UPDATE watchdog_lag SET lag_sec = $1"
	watchdog_wait_for_replay
	watchdog_load watchdog_after_poll
	watchdog_after_poll_loaded=true
	watchdog_load watchdog_before_poll
	watchdog_before_poll_loaded=false
	watchdog_wait_for_point watchdog_after_poll
}
