-- a statement prepared with the extended protocol stays usable
-- after the client moves to other backends

select $1::int as v \parse switch_pstmt

\bind_named switch_pstmt 1
\g

-- move to another backend: terminate the current one directly in
-- postgres and wait for it to exit; the next query gets another backend
select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

\bind_named switch_pstmt 2
\g

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

\bind_named switch_pstmt 3
\g
