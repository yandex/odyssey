-- read-your-writes: 1 primary (localhost:5432) and 4 replicas
-- (localhost:5433..5436)
SET odyssey.execute_on_host TO 'localhost:5432';
SET application_name TO 'rwr_5node';
CREATE TABLE rwr_test (i int);
-- replicate the empty table, then confirm replay has actually stopped
\! ${PGTEST_DIR}/sql/replay.sh pause
SET odyssey.rwr_consistent TO 'on';
SHOW odyssey.rwr_consistent;
INSERT INTO rwr_test (i) VALUES (42);
-- setting an already enabled mode must preserve the captured LSN
SET odyssey.rwr_consistent TO 'on';
-- resume replay only after observing Odyssey blocked on WAIT FOR LSN
\! ${PGTEST_DIR}/sql/replay.sh observe &
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT count(*) FROM rwr_test;
\! ${PGTEST_DIR}/sql/replay.sh check
SELECT i FROM rwr_test;
SET odyssey.execute_on_host TO DEFAULT;
SET odyssey.rwr_consistent TO 'off';
SHOW odyssey.rwr_consistent;
SET odyssey.execute_on_host TO 'localhost:5432';
DROP TABLE rwr_test;
SET odyssey.execute_on_host TO DEFAULT;
