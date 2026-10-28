-- names starting with odyssey_pstmt_ belong to odyssey's own statements
-- on backends and are rejected

prepare odyssey_pstmt_1 as select 1;
\echo :SQLSTATE
execute odyssey_pstmt_1;
execute U&"odyssey\005fpstmt_1";
explain analyze create table sql_reserved as execute odyssey_pstmt_1;
deallocate odyssey_pstmt_1;

-- not a statement name: allowed
select 'odyssey_pstmt_1' as v;

-- quoted names are case-sensitive: this one is not reserved, while the
-- same name unquoted folds to lower case and is
prepare "ODYSSEY_PSTMT_1" as select 'quoted' as v;
execute "ODYSSEY_PSTMT_1";
execute ODYSSEY_PSTMT_1;
deallocate "ODYSSEY_PSTMT_1";

-- A whole batch must be rejected before its first side effect. A sequence
-- change would survive the implicit transaction rollback after an error.
create sequence sql_reserved_batch;
select nextval('sql_reserved_batch') \; execute odyssey_pstmt_1;
\echo :SQLSTATE
select is_called from sql_reserved_batch;
drop sequence sql_reserved_batch;

-- Valid quoted/Unicode names and wrapped EXECUTE still reach PostgreSQL.
begin;
prepare U&"na\006de" as select 'unicode' as v;
execute U&"na!006de" UESCAPE '!';
explain (costs off) execute name;
create temp table sql_reserved_valid as execute name;
select * from sql_reserved_valid;
prepare "na""me" as select 'quoted identifier' as v;
execute "na""me";
rollback;

-- The namespace is only reserved when prepared statement reservation is on.
\connect 'host=localhost port=6432 user=tuser_unreserved dbname=postgres'
prepare odyssey_pstmt_1 as select 'not reserved' as v;
execute odyssey_pstmt_1;
deallocate odyssey_pstmt_1;
