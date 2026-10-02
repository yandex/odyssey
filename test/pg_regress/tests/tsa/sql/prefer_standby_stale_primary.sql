\connect 'host=localhost port=6432 user=postgres dbname=postgres'

-- refresh the primary status, then let it expire
-- (endpoints_status_poll_interval is 5s in the config)
SET odyssey.target_session_attrs TO 'read-write';
SET odyssey.execute_on_host TO 'localhost:5432';
select pg_is_in_recovery();
\! sleep 6

-- refresh the replicas only: the primary status stays outdated
SET odyssey.target_session_attrs TO 'read-only';
SET odyssey.execute_on_host TO 'localhost:5433';
select pg_is_in_recovery();
SET odyssey.execute_on_host TO 'localhost:5434';
select pg_is_in_recovery();
SET odyssey.execute_on_host TO DEFAULT;

-- prefer-standby must not end up on the primary just because its status is stale
SET odyssey.target_session_attrs TO 'prefer-standby';
select pg_is_in_recovery();
select pg_is_in_recovery();
select pg_is_in_recovery();
select pg_is_in_recovery();
