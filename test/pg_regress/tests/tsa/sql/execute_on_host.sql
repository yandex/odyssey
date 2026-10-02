\connect 'host=localhost port=6432 user=postgres dbname=postgres'

-- not set by default
SHOW odyssey.execute_on_host;

-- pin the session to the primary: no round-robin anymore
SET odyssey.execute_on_host TO 'localhost:5432';
SHOW odyssey.execute_on_host;
select pg_is_in_recovery();
select pg_is_in_recovery();
select pg_is_in_recovery();

-- pin to a replica
SET odyssey.execute_on_host = 'localhost:5433';
SHOW odyssey.execute_on_host;
select pg_is_in_recovery();
select pg_is_in_recovery();
select pg_is_in_recovery();

-- and to another one
SET odyssey.execute_on_host TO 'localhost:5434';
select pg_is_in_recovery();
select pg_is_in_recovery();

-- hosts unknown to the storage are rejected
SET odyssey.execute_on_host TO 'localhost:5430';
SET odyssey.execute_on_host TO 'nonexistent';
SET odyssey.execute_on_host TO 'localhost:5432,localhost:5433';
SHOW odyssey.execute_on_host;

-- reset: back to round-robin over all hosts
SET odyssey.execute_on_host TO DEFAULT;
SHOW odyssey.execute_on_host;
select 1;
select 1;
select 1;
