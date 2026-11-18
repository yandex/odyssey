# Keep the real ROLLBACK paused while another client waits for the only backend.
setup
{
  CREATE EXTENSION dblink;
  CREATE EXTENSION injection_points;
  CREATE TABLE rollback_data (i int);
  CREATE TABLE rollback_backend (pid int);
  CREATE VIEW rollback_pool AS
    SELECT cl_queue, sv_active, sv_idle
    FROM dblink('host=localhost port=6432 dbname=console user=postgres', 'SHOW POOLS_EXTENDED')
      AS p(db text, username text, cl_active bigint, cl_waiting bigint,
           sv_active bigint, sv_idle bigint, sv_used bigint, sv_tested bigint,
           sv_login bigint, maxwait bigint, maxwait_us bigint, pool_mode text,
           bytes_received bigint, bytes_sent bigint, tcp_conn_count bigint, cl_queue bigint,
           client_connections bigint, client_disconnections bigint,
           server_connections bigint, server_disconnections bigint, server_detaches bigint)
    WHERE db = 'postgres' AND cl_queue > 0;
  CREATE FUNCTION wait_for_rollback() RETURNS void LANGUAGE plpgsql AS $$
  DECLARE deadline timestamptz := clock_timestamp() + interval '5 seconds';
  BEGIN
    WHILE clock_timestamp() < deadline LOOP
      PERFORM pg_stat_clear_snapshot();
      IF EXISTS (SELECT FROM pg_stat_activity
                 WHERE application_name = 'rollback-owner'
                   AND query = 'ROLLBACK'
                   AND wait_event = 'transaction-abort-after-clog')
         AND EXISTS (SELECT FROM rollback_pool) THEN
        RETURN;
      END IF;
      PERFORM pg_sleep(0.001);
    END LOOP;
    RAISE EXCEPTION 'ROLLBACK or pool waiter did not reach the barrier';
  END;
  $$;
}

teardown
{
  DROP FUNCTION wait_for_rollback();
  DROP VIEW rollback_pool;
  DROP TABLE rollback_data, rollback_backend;
  DROP EXTENSION dblink;
  DROP EXTENSION injection_points;
}

session "clients"
setup
{
  BEGIN;
  SELECT dblink_connect('owner', 'host=localhost port=6432 dbname=postgres user=rollback application_name=rollback-owner');
  SELECT dblink_connect('next', 'host=localhost port=6432 dbname=postgres user=rollback');
  SELECT dblink_exec('owner', 'INSERT INTO rollback_backend SELECT pg_backend_pid()');
}
step "write"
{
  SELECT dblink_exec('owner', $q$
    BEGIN;
    INSERT INTO rollback_data VALUES (1);
    DO $body$ BEGIN
      PERFORM injection_points_set_local();
      PERFORM injection_points_attach('transaction-abort-after-clog', 'wait');
    END $body$;
  $q$);
}
step "next_query"
{
  SELECT dblink_send_query('next', 'SELECT pg_backend_pid() = (SELECT pid FROM rollback_backend) AS same_backend, count(*) AS rows FROM rollback_data');
}
step "disconnect" { SELECT dblink_disconnect('owner'); }
step "invariants"
{
  SELECT dblink_is_busy('next');
  SELECT * FROM rollback_pool;
}
step "next_result"
{
  SELECT * FROM dblink_get_result('next') AS r(same_backend boolean, rows bigint);
}

teardown { SELECT dblink_disconnect('next'); COMMIT; }

session "control"
step "rollback_paused" { SELECT wait_for_rollback(); }
step "wake"
{
  SELECT FROM injection_points_detach('transaction-abort-after-clog');
  SELECT FROM injection_points_wakeup('transaction-abort-after-clog');
}

permutation write next_query disconnect rollback_paused invariants wake next_result
