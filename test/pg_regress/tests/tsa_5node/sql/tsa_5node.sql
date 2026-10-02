-- 1 primary (localhost:5432) and 4 replicas (localhost:5433..5436)
SET odyssey.target_session_attrs TO 'read-write';
SET odyssey.execute_on_host TO 'localhost:5432';
SHOW odyssey.execute_on_host;
CREATE TABLE tsa_test (i int);
INSERT INTO tsa_test (i) VALUES (22);
SELECT count(*) FROM tsa_test;
SELECT 1+2;
SET odyssey.target_session_attrs TO 'prefer-standby';
SET odyssey.execute_on_host TO 'localhost:5433';
SELECT 1+2;
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT 1+2;
SET odyssey.execute_on_host TO 'localhost:5435';
SELECT 1+2;
SET odyssey.execute_on_host TO 'localhost:5436';
SELECT 1+2;
SET odyssey.target_session_attrs TO 'read-only';
SET odyssey.execute_on_host TO 'localhost:5433';
SELECT pg_is_in_recovery();
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT pg_is_in_recovery();
SET odyssey.execute_on_host TO 'localhost:5435';
SELECT pg_is_in_recovery();
SET odyssey.execute_on_host TO 'localhost:5436';
SELECT pg_is_in_recovery();
SET odyssey.target_session_attrs TO 'any';
SET odyssey.execute_on_host TO 'localhost:5432';
SELECT pg_is_in_recovery();
SET odyssey.execute_on_host TO DEFAULT;
-- cut the replicas off: prefer-standby must fall back to the primary
\! ${PGTEST_DIR}/sql/replicas.sh stop
\! sleep 1
SET odyssey.target_session_attrs TO 'prefer-standby';
SELECT pg_is_in_recovery();
SELECT pg_is_in_recovery();
SET odyssey.target_session_attrs TO 'read-write';
SET odyssey.execute_on_host TO 'localhost:5432';
SELECT 1+2;
SET odyssey.execute_on_host TO DEFAULT;
-- pinned dead replica is an error (child psql, server id is masked)
\! psql 'host=localhost port=6432 user=postgres dbname=postgres' -c "SET odyssey.execute_on_host TO 'localhost:5433'" -c 'select 1' 2>&1 | sed -E 's/remote server s[0-9a-f]+/remote server <id>/'
\! ${PGTEST_DIR}/sql/replicas.sh start
SET odyssey.target_session_attrs TO 'read-only';
SET odyssey.execute_on_host TO 'localhost:5433';
SELECT pg_is_in_recovery();
SET odyssey.execute_on_host TO 'localhost:5434';
SELECT pg_is_in_recovery();
SET odyssey.execute_on_host TO 'localhost:5435';
SELECT pg_is_in_recovery();
SET odyssey.execute_on_host TO 'localhost:5436';
SELECT pg_is_in_recovery();
SET odyssey.target_session_attrs TO 'read-write';
SET odyssey.execute_on_host TO 'localhost:5432';
DROP TABLE tsa_test;
