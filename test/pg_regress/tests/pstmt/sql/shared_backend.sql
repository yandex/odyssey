-- clients of a single-backend route use the same backend in turn;
-- each sees only its own statements, even under the same name

\connect 'host=localhost port=6432 user=tuser_one dbname=postgres'

select 'mine' as v \parse shared_pstmt

\! psql -X -q -At "host=localhost port=6432 user=tuser_one dbname=postgres" -f ${PGTEST_DIR}/sql/shared_backend_other_client.sql

\bind_named shared_pstmt
\g

select 'only mine' as v \parse own_pstmt

\! psql -X -q -At "host=localhost port=6432 user=tuser_one dbname=postgres" -c "execute own_pstmt"
