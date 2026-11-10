-- A SQL alias can be used by both SQL EXECUTE and protocol Bind.
PREPARE poc_sql AS SELECT $1::int AS v;
BEGIN;
SELECT count(*) AS copies, bool_and(NOT from_sql) AS protocol_parse
FROM pg_prepared_statements WHERE statement = 'SELECT $1::int AS v';
COMMIT;
EXECUTE poc_sql(7);
\bind_named poc_sql 8
\g

-- The equivalent Parse has zero OIDs and shares the same backend statement.
SELECT $1::int AS v \parse poc_protocol
BEGIN;
SELECT count(*) AS copies, bool_and(NOT from_sql) AS protocol_parse
FROM pg_prepared_statements WHERE statement = 'SELECT $1::int AS v';
COMMIT;

-- A new backend receives the same protocol Parse on first use.
SELECT pg_backend_pid() AS old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
SELECT pg_backend_pid() <> :old_pid AS switched;
EXECUTE poc_sql(9);
\bind_named poc_sql 10
\g

DEALLOCATE poc_sql;
EXECUTE poc_protocol(11);
\close_prepared poc_protocol
