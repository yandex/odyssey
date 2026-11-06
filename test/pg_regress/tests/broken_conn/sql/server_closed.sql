select 43;

set application_name to 'kill_me';

\! ${PGTEST_DIR}/sql/kill_server.sh

\! sleep 1

select 87;

-- idle pooled connection with pending data but no close:
-- 'LISTEN' survives return to the pool (pool_discard no), the
-- 'NOTIFY' below makes postgres send a NotificationResponse to
-- the pooled idle backend, so its socket becomes readable while
-- still open. attach_check must catch that and retry the attach.
listen odyssey_regress_pending_data;

\! psql "host=localhost port=5432 user=postgres dbname=postgres" -c "NOTIFY odyssey_regress_pending_data"

\! sleep 1

select 121;
