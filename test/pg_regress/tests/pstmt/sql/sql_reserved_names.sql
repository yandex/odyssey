-- names starting with odyssey_pstmt_ belong to odyssey's own statements
-- on backends and are rejected

prepare odyssey_pstmt_1 as select 1;
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
