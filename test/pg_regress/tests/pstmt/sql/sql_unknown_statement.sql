-- a statement created through dynamic SQL is invisible to odyssey; if it
-- takes the name of a client's statement on the shared backend, odyssey
-- can not prepare the client's one there and moves the client to a new
-- backend instead of exposing the hidden statement

\connect 'host=localhost port=6432 user=tuser_one dbname=postgres'

prepare sql_taken as select 'tracked' as v;

\! psql -X -q -At "host=localhost port=6432 user=tuser_one dbname=postgres" -f ${PGTEST_DIR}/sql/sql_unknown_statement_other_client.sql

execute sql_taken;
