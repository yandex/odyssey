begin;

select 53;

\! sleep 1
select 73;

-- long op is not broken
select pg_sleep(6);

\! sleep 6

select 76;

commit;
