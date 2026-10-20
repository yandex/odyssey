-- run by shared_backend.sql as another client of the same route
select 'other' as v \parse shared_pstmt
\bind_named shared_pstmt
\g
