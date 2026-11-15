-- Listeners have independent defaults; an omitted setting remains off.
\connect 'postgresql://postgres@localhost:6432/postgres?options='
SHOW odyssey.rwr_consistent;
\connect 'postgresql://postgres@localhost:7433/postgres?options='
SHOW odyssey.rwr_consistent;
\connect 'postgresql://postgres@localhost:7432/postgres?options='
SHOW odyssey.rwr_consistent;
-- The listener default does not force the setting after connection startup.
SET odyssey.rwr_consistent TO off;
SHOW odyssey.rwr_consistent;
-- Startup options override both enabled and disabled listener defaults.
\connect 'postgresql://postgres@localhost:7432/postgres?options=-c%20odyssey.rwr_consistent%3Doff'
SHOW odyssey.rwr_consistent;
\connect 'postgresql://postgres@localhost:7433/postgres?options=-c%20odyssey.rwr_consistent%3Don'
SHOW odyssey.rwr_consistent;
-- A fresh client inherits the enabled default and waits without a SET to on.
\connect 'postgresql://postgres@localhost:7432/postgres?options='
SHOW odyssey.rwr_consistent;
SET odyssey.execute_on_host TO 'localhost:5432';
SET application_name TO 'rwr_5node';
CREATE TABLE rwr_test (i int);
\! ${PGTEST_DIR}/sql/replay.sh pause
INSERT INTO rwr_test VALUES (42);
\! ${PGTEST_DIR}/sql/replay.sh observe &
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT count(*) FROM rwr_test;
\! ${PGTEST_DIR}/sql/replay.sh check
SET odyssey.execute_on_host TO 'localhost:5432';
DROP TABLE rwr_test;
