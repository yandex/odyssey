\connect 'host=localhost port=6432 user=rouser dbname=postgres'

select pg_is_in_recovery();

select pg_is_in_recovery();

select pg_is_in_recovery();

\connect 'host=localhost port=6432 user=rwuser dbname=postgres'

select pg_is_in_recovery();

select pg_is_in_recovery();

select pg_is_in_recovery();
