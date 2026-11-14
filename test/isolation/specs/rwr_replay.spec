# A paused replica must not answer this client's read before replay resumes.
setup { CREATE TABLE isolation_rwr (i integer); }
teardown { DROP TABLE isolation_rwr; }

session "client"
setup { SET odyssey.rwr_consistent TO 'on'; }
step "write" { INSERT INTO isolation_rwr VALUES (42); }
step "read_only" { SET odyssey.target_session_attrs TO 'read-only'; }
step "read" { SELECT pg_is_in_recovery() AS on_replica, i FROM isolation_rwr; }

session "replay"
setup { SET odyssey.target_session_attrs TO 'read-only'; }
step "pause" {
  DO $$
  DECLARE
    deadline timestamptz := clock_timestamp() + interval '10 seconds';
  BEGIN
    PERFORM pg_wal_replay_pause();
    WHILE pg_get_wal_replay_pause_state() <> 'paused' LOOP
      IF clock_timestamp() >= deadline THEN
        RAISE EXCEPTION 'replica replay did not pause';
      END IF;
      PERFORM pg_sleep(0.01);
    END LOOP;
  END $$;
}
step "resume" {
  DO $$
  DECLARE
    deadline timestamptz := clock_timestamp() + interval '10 seconds';
  BEGIN
    IF pg_get_wal_replay_pause_state() <> 'paused' THEN
      RAISE EXCEPTION 'replica replay resumed before the read';
    END IF;
    LOOP
      PERFORM pg_stat_clear_snapshot();
      EXIT WHEN EXISTS (
        SELECT FROM pg_stat_activity
        WHERE application_name LIKE 'odyssey-isolation/%'
          AND state = 'active'
          AND query LIKE 'WAIT FOR LSN %'
          AND wait_event = 'WaitForWalReplay'
      );
      IF clock_timestamp() >= deadline THEN
        RAISE EXCEPTION 'client did not wait for WAL replay';
      END IF;
      PERFORM pg_sleep(0.01);
    END LOOP;
    PERFORM pg_wal_replay_resume();
  EXCEPTION WHEN OTHERS THEN
    PERFORM pg_wal_replay_resume();
    RAISE;
  END $$;
}
teardown { DO $$ BEGIN PERFORM pg_wal_replay_resume(); END $$; }

# (*) lets the controller run despite WAIT not being a heavyweight lock wait.
# The resume step asserts the actual backend wait before releasing the reader.
permutation pause write read_only read(*) resume
