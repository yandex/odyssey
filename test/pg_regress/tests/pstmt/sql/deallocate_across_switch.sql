-- SQL DEALLOCATE of a statement prepared with the extended protocol

select 1 as v \parse dealloc_pstmt
deallocate dealloc_pstmt;

-- move to another backend: terminate the current one directly in
-- postgres and wait for it to exit; the next query gets another backend
select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

\bind_named dealloc_pstmt
\g

select 1 as v \parse dealloc_all_1
select 2 as v \parse dealloc_all_2
deallocate all;

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

\bind_named dealloc_all_1
\g
\bind_named dealloc_all_2
\g
