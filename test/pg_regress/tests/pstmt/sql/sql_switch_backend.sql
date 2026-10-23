-- statements created by SQL PREPARE stay usable after the client moves
-- to other backends

prepare sql_switch (int) as select $1 * 10 as v;
execute sql_switch(1);

-- move to another backend: terminate the current one directly in
-- postgres and wait for it to exit; the next query gets another backend
select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute sql_switch(2);

-- several EXECUTEs in one query, as psycopg2 execute_batch sends them
prepare sql_batch_a (int) as select $1 as a;
prepare sql_batch_b (int) as select $1 * 2 as b;

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute sql_batch_a(1) \; execute sql_batch_b(2) \; execute sql_switch(3);

-- several PREPAREs in one query, quoted names
select 1 as one \; prepare sql_multi as select 'm' as v \; prepare "Sql ""Quoted""" as select 'q' as v;

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute sql_multi;
execute "Sql ""Quoted""";

-- names differing only in case are different statements
prepare "Case" as select 'quoted' as v;
prepare case_stmt as select 'unquoted' as v;
prepare "case_stmt_2" as select 'quoted lower' as v;

select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;

execute "Case";
execute case_stmt;
execute CASE_STMT;
execute CASE_STMT_2;
execute "CASE_STMT";
