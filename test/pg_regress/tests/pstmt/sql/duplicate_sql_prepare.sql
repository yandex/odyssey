\pset format unaligned
\set VERBOSITY sqlstate

-- A protocol Parse and SQL PREPARE use the same client namespace.
SET odyssey.parser_mode = 'mini';
SELECT 1 AS v \parse duplicate_mini
-- PostgreSQL checks the body and parameter types before the duplicate name.
PREPARE duplicate_mini AS SELECT * FROM nonexistent_prepare_relation;
PREPARE duplicate_mini (unknown_missing_type) AS SELECT 2;
PREPARE duplicate_mini AS SELECT 2;
\set VERBOSITY default
PREPARE duplicate_mini AS SELECT 2;
\set VERBOSITY sqlstate
EXECUTE duplicate_mini;

-- A different backend must still see the collision; no shadow may remain.
SELECT pg_backend_pid() AS old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
SELECT pg_backend_pid() <> :old_pid AS switched;
PREPARE duplicate_mini AS SELECT 2;
BEGIN;
SELECT count(*) AS shadows FROM pg_prepared_statements WHERE statement = 'SELECT 1';
COMMIT;
EXECUTE duplicate_mini;

-- The duplicate must abort a transaction, including a deferred BEGIN.
SELECT pg_backend_pid() AS old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
BEGIN;
PREPARE duplicate_mini (int) AS SELECT $1;
SELECT 3;
ROLLBACK;
EXECUTE duplicate_mini;
\close_prepared duplicate_mini

-- Once the protocol statement is closed the SQL name is available.
BEGIN;
PREPARE duplicate_mini AS SELECT 2 AS v;
EXECUTE duplicate_mini;
DEALLOCATE duplicate_mini;
COMMIT;

-- Full parser resolves quoted and Unicode-escaped identifiers.
SET odyssey.parser_mode = 'full';
SELECT 4 AS v \parse MixedCase
PREPARE "MixedCase" AS SELECT 5;
PREPARE U&"\004DixedCase" AS SELECT 5;
PREPARE U&"!004DixedCase" UESCAPE '!' AS SELECT 5;
EXECUTE "MixedCase";
\close_prepared MixedCase
