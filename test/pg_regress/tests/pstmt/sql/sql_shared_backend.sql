-- clients of a single-backend route use the same backend in turn;
-- each sees only its own SQL-prepared statements

\connect 'host=localhost port=6432 user=tuser_one dbname=postgres'

prepare sql_private as select 'mine' as v;

\! psql -X -q -At "host=localhost port=6432 user=tuser_one dbname=postgres" -c "execute sql_private"

\! psql -X -q -At "host=localhost port=6432 user=tuser_one dbname=postgres" -c "prepare sql_private as select 'other' as v; execute sql_private"

execute sql_private;

-- statements of a closed client do not stay on the backend
\! psql -X -q -At "host=localhost port=6432 user=tuser_one dbname=postgres" -c "prepare sql_orphan as select 1"

select count(*) from pg_prepared_statements where name = 'sql_orphan';
