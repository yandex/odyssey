-- SQL PREPARE and protocol Parse share a statement namespace
select 1 as v \parse shared_name
prepare shared_name as select 2 as v;
\bind_named shared_name
\g

-- DEALLOCATE frees the protocol name for SQL PREPARE
deallocate shared_name;
prepare shared_name as select 3 as v;
select 4 as v \parse shared_name
execute shared_name;

-- SQL DEALLOCATE frees the name for protocol Parse too
deallocate shared_name;
select 5 as v \parse shared_name
\bind_named shared_name
\g

-- DEALLOCATE ALL can free the name in the same simple query
deallocate all \; prepare shared_name as select 6 as v;
execute shared_name;
deallocate all;
