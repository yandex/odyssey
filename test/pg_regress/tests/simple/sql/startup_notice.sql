DROP DATABASE IF EXISTS broken_collation;
CREATE DATABASE broken_collation;
UPDATE pg_database SET datcollversion = 'bogus-version' WHERE datname = 'broken_collation';

\connect broken_collation

select 42;

\connect postgres
-- drop connections to avoid error of 'There is 1 other session using the database.'
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = 'broken_collation';

DROP DATABASE IF EXISTS broken_collation;
