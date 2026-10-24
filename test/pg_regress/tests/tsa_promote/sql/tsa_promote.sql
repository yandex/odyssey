-- 1 primary (localhost:5432) and 4 replicas (localhost:5433..5436).
-- Replica 5434 is put down, replica 5433 is promoted to a new master:
-- after the promote the writer must go to the promoted host.
SET odyssey.target_session_attrs TO 'read-write';
SET odyssey.execute_on_host TO 'localhost:5432';
CREATE TABLE tsa_promote_test (i int);
INSERT INTO tsa_promote_test VALUES (22);
SELECT count(*) FROM tsa_promote_test;
-- pinned read-write to a replica does not match the target attrs
-- (child psql, server id is masked)
\! psql "host=localhost port=${PGTEST_PORT} user=postgres dbname=postgres" -c "SET odyssey.target_session_attrs TO 'read-write'" -c "SET odyssey.execute_on_host TO 'localhost:5433'" -c 'select 1' 2>&1 | sed -E 's/remote server s[0-9a-f]+/remote server <id>/'
-- put replica 5434 down, promote replica 5433 to a new master
\! ${PGTEST_DIR}/sql/promote.sh stop
\! ${PGTEST_DIR}/sql/promote.sh promote
-- the writer goes to the promoted host now
SET odyssey.execute_on_host TO 'localhost:5433';
SELECT pg_is_in_recovery();
INSERT INTO tsa_promote_test VALUES (23);
SELECT count(*) FROM tsa_promote_test;
SET odyssey.execute_on_host TO DEFAULT;
-- the put down replica is dead (child psql, server id is masked)
\! psql "host=localhost port=${PGTEST_PORT} user=postgres dbname=postgres" -c "SET odyssey.execute_on_host TO 'localhost:5434'" -c 'select 1' 2>&1 | sed -E 's/remote server s[0-9a-f]+/remote server <id>/'
-- the remaining replicas still serve reads
SET odyssey.target_session_attrs TO 'read-only';
SET odyssey.execute_on_host TO 'localhost:5435';
SELECT pg_is_in_recovery();
SET odyssey.execute_on_host TO 'localhost:5436';
SELECT pg_is_in_recovery();
SET odyssey.target_session_attrs TO 'read-write';
SET odyssey.execute_on_host TO 'localhost:5433';
DROP TABLE tsa_promote_test;
