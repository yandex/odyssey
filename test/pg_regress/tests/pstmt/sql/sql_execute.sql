-- SQL EXECUTE resolves the client alias of a protocol Parse.
\pset format unaligned
select $1::int as v, $2::text as s \parse exec_expr
EXECUTE exec_expr(1, 'text');
select 42 as v \parse MixedCase
EXECUTE "MixedCase";

-- A different backend has to receive the saved protocol Parse first.
select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;
/* before */ EXECUTE /* name */ exec_expr /* args */ (2, $$я;')$$);

-- Both entry points keep using the same protocol statement.
\bind_named exec_expr 7 text
\g

-- Names absent from the protocol map keep native SQL behavior.
BEGIN;
PREPARE native_exec AS SELECT 99 AS v;
EXECUTE native_exec;
DEALLOCATE native_exec;
COMMIT;
\close_prepared MixedCase
\set VERBOSITY sqlstate
EXECUTE "MixedCase";
