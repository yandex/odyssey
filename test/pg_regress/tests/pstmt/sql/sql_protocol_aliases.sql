-- SQL and protocol aliases of identical query bytes share one backend statement
begin;
select 72631 as alias_value\parse proto_alias
prepare sql_alias as select 72631 as alias_value;
execute proto_alias;
execute sql_alias;
\bind_named sql_alias
\g
select count(*) as backend_copies, bool_and(not from_sql) as protocol_deploy
from pg_prepared_statements where statement like '%72631%';
commit;

-- Reuse works in the other order as well
begin;
prepare sql_first as select 72632 as alias_value;
select 72632 as alias_value\parse proto_second
execute proto_second;
select count(*) as backend_copies, bool_and(not from_sql) as protocol_deploy
from pg_prepared_statements where statement like '%72632%';
commit;

-- A duplicate SQL name reports the original name and aborts the transaction
begin;
select 1;
prepare sql_alias as select 0;
select 2;
rollback;
execute sql_alias;

-- Parameter types are inferred from the body for Bind and backend redeploy
prepare sql_typed as select $1::numeric as amount, $2::text[] as labels;
\bind_named sql_typed 12.34 '{a,b}'
\g
select pg_backend_pid() as old_pid \gset
\setenv OLD_PID :old_pid
\! psql "host=localhost port=5432 user=postgres dbname=postgres" -Atqc "select pg_terminate_backend($OLD_PID, 10000)"
select pg_backend_pid() <> :old_pid as switched;
\bind_named sql_typed 56.78 '{c,d}'
\g
execute sql_typed(90.12, '{e,f}');

-- SQL EXECUTE binds the cached body directly, without an SQL EXECUTE wrapper
prepare direct_execute as select current_query() like 'select %' as protocol_execute;
execute direct_execute;

-- Literal values are sent as text Bind parameters; NULL keeps its protocol marker
prepare literal_args as select $1::int as i, $2::numeric as n, $3::text as v,
    $4::boolean as b, $5::text is null as was_null;
execute literal_args(-42, 12.34, 'a''b; --', true, NULL);
execute literal_args(42, -12.34, $$a;b$$, false, '');
begin;
execute literal_args(42);
select 1;
rollback;
execute direct_execute;

-- Expressions are rejected before any preceding statement is executed
create sequence execute_batch_effect;
select nextval('execute_batch_effect') \; execute literal_args(1 + 2, 0, '', false, NULL);
\echo :SQLSTATE
select is_called from execute_batch_effect;
drop sequence execute_batch_effect;

-- Completed PREPARE survives a later failure in the same simple-query batch
prepare batch_ok as select 'prefix' as v \; select 1/0 \; prepare batch_not_run as select 0;
execute batch_ok;
execute batch_not_run;

-- Parse must run after preceding DDL in the same simple-query batch
create table prepare_batch_table (v int) \; prepare batch_schema as select * from prepare_batch_table \; execute batch_schema;
drop table prepare_batch_table;

-- PREPARE participates in the surrounding implicit transaction block
prepare implicit_keep as select 72633 as v \; discard all;
execute implicit_keep;

-- Ordinary quoted names preserve case
prepare "Name" as select 7 as v;
execute "Name";

-- An explicit parameter type list is unsupported, including inside a batch
prepare explicit_types (int) as select $1;
\echo :SQLSTATE
execute explicit_types(1);
create sequence typed_batch_effect;
select nextval('typed_batch_effect') \; prepare explicit_types (int) as select $1;
select is_called from typed_batch_effect;
drop sequence typed_batch_effect;

-- Unsupported forms are rejected before any part of the batch is sent
prepare U&"na!006de" UESCAPE '!' as select 8 as v;
execute U&"na\006de";
deallocate U&"na!006de" UESCAPE '!';
explain execute "Name";
create table sql_alias_result as execute "Name";
prepare rejected_batch as select 9 \; execute U&"na\006de";
execute rejected_batch;
execute "Name";

-- Large backend packets pass through the SQL alias response path intact
prepare sql_large as select length(repeat('v', 100000)) as payload_length, repeat('v', 100000) as payload;
execute sql_large \gset
\echo :payload_length

deallocate all;
