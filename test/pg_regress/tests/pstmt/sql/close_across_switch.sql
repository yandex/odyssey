-- a closed statement stays closed on other backends,
-- and its name can be prepared again

select 1 as v \parse close_pstmt
\close_prepared close_pstmt

-- move to another backend: terminate the current one directly in
-- postgres and wait for it to exit; the next query gets another backend
select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

\bind_named close_pstmt
\g

select 2 as v \parse close_pstmt

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

\bind_named close_pstmt
\g
