-- read-your-writes: 1 primary (localhost:5432) and 4 replicas
-- (localhost:5433..5436)
SET odyssey.execute_on_host TO 'localhost:5432';
SET application_name TO 'rwr_5node';
CREATE TABLE rwr_test (i int);
CREATE FUNCTION rwr_write(int) RETURNS void LANGUAGE SQL AS $$ INSERT INTO rwr_test VALUES ($1) $$;
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
-- A SELECT can write WAL, even though the full parser classifies it as a read.
SET odyssey.parser_mode TO 'full';
SET odyssey.execute_on_host TO 'localhost:5432';
\! ${PGTEST_DIR}/sql/replay.sh pause
SELECT rwr_write(43);
\! ${PGTEST_DIR}/sql/replay.sh observe &
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT count(*) FROM rwr_test;
\! ${PGTEST_DIR}/sql/replay.sh check
-- An unclassified extended-protocol write and read must use the same barrier.
SET odyssey.parser_mode TO 'mini';
SET odyssey.execute_on_host TO 'localhost:5432';
\! ${PGTEST_DIR}/sql/replay.sh pause
INSERT INTO rwr_test VALUES (44) \bind \g
\! ${PGTEST_DIR}/sql/replay.sh observe &
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT count(*) FROM rwr_test \bind \g
\! ${PGTEST_DIR}/sql/replay.sh check
-- Exceeding query_parsing_max_query_len must not disable LSN capture or wait.
SET odyssey.execute_on_host TO 'localhost:5432';
\! ${PGTEST_DIR}/sql/replay.sh pause
INSERT INTO rwr_test VALUES (45) /* This query deliberately exceeds the parser length limit of 128 bytes: its write must still advance the consistency barrier. */;
\! ${PGTEST_DIR}/sql/replay.sh observe &
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT count(*) FROM rwr_test /* This query also exceeds the parser length limit of 128 bytes: its read must still wait until the replica has replayed our write. */;
\! ${PGTEST_DIR}/sql/replay.sh check
-- WAIT must precede the deferred BEGIN and its first snapshot-taking query.
SET odyssey.execute_on_host TO 'localhost:5432';
\! ${PGTEST_DIR}/sql/replay.sh pause
INSERT INTO rwr_test VALUES (46);
\! ${PGTEST_DIR}/sql/replay.sh observe &
SET odyssey.execute_on_host TO 'localhost:5434';
SET odyssey.parser_mode TO 'full';
BEGIN ISOLATION LEVEL REPEATABLE READ;
SELECT count(*) FROM rwr_test \bind \g
\! ${PGTEST_DIR}/sql/replay.sh check
COMMIT;
SET odyssey.execute_on_host TO DEFAULT;
SET odyssey.rwr_consistent TO 'off';
SHOW odyssey.rwr_consistent;
SET odyssey.execute_on_host TO 'localhost:5432';
DROP FUNCTION rwr_write(int);
DROP TABLE rwr_test;
SET odyssey.execute_on_host TO DEFAULT;
