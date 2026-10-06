\connect 'host=localhost port=8433 user=rruser dbname=postgres'

select pg_is_in_recovery() \parse ext_ro_stmt
\bind_named ext_ro_stmt
\g

select pg_is_in_recovery() \parse ext_ro_stmt2
\g

\bind_named ext_ro_stmt2
\g

select pg_is_in_recovery() \bind
\g

create temporary table auto_ro_ext_baz(i int) \parse ext_rw_stmt
\bind_named ext_rw_stmt
\g

-- will not produce the NOTICE because of pinning on temporary table
drop table auto_ro_ext_baz \parse ext_rw_stmt2
\bind_named ext_rw_stmt2
\g

select pg_is_in_recovery() \parse ext_ro_stmt3
\bind_named ext_ro_stmt3
\g
