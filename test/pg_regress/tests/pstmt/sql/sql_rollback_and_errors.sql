-- PREPARE is not transactional: it survives a rollback
begin;
prepare sql_rb as select 42 as v;
rollback;

-- a failed PREPARE leaves no statement behind
prepare sql_bad as select * from sql_no_such_table;

-- a duplicate PREPARE fails as in a direct session
prepare sql_rb as select 1;

-- move to another backend: terminate the current one directly in
-- postgres and wait for it to exit; the next query gets another backend
select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute sql_rb;
execute sql_bad;

prepare sql_rb as select 2;
prepare sql_bad as select 'fixed' as v;
execute sql_bad;
