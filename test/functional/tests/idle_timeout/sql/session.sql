-- no connection acquired
\! sleep 3

-- long active query works
select pg_sleep(6);

select 43;

\! sleep 6

select 87;
