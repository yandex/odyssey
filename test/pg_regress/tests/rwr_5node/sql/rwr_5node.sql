-- read-your-writes: 1 primary (localhost:5432) and 4 replicas
-- (localhost:5433..5436)
SET odyssey.rwr_consistent TO 'on';
SHOW odyssey.rwr_consistent;
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT pg_wal_replay_pause();
SET odyssey.execute_on_host TO 'localhost:5432';
CREATE TABLE rwr_test (i int);
INSERT INTO rwr_test (i) VALUES (42);
-- resume the paused replica replay in the background: the next SELECT
-- must block on WAIT FOR LSN until the INSERT is replayed, so it sees
-- the just-inserted row instead of the pre-INSERT state
\! ( sleep 3; psql -X -q -h 127.0.0.1 -p 5434 -U postgres -d postgres -c "SELECT pg_wal_replay_resume();" >/dev/null 2>&1 ) &
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT count(*) FROM rwr_test;
SELECT i FROM rwr_test;
SET odyssey.execute_on_host TO DEFAULT;
SET odyssey.rwr_consistent TO 'off';
SHOW odyssey.rwr_consistent;
SET odyssey.execute_on_host TO 'localhost:5432';
DROP TABLE rwr_test;
SET odyssey.execute_on_host TO DEFAULT;
