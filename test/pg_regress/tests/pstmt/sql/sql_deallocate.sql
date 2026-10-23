-- DEALLOCATE, DEALLOCATE ALL and DISCARD ALL reach the backend and stay
-- in effect on other backends

prepare sql_d1 as select 1 as v;
deallocate sql_d1;

-- move to another backend: terminate the current one directly in
-- postgres and wait for it to exit; the next query gets another backend
select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute sql_d1;
prepare sql_d1 as select 2 as v;
execute sql_d1;

prepare sql_d2 as select 3 as v;
deallocate all;

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute sql_d1;
execute sql_d2;

prepare sql_d3 as select 4 as v;
discard all;

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute sql_d3;

-- a DISCARD ALL rejected by the backend changes nothing
prepare sql_keep as select 'kept' as v;
begin;
select 1;
discard all;
rollback;

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute sql_keep;
