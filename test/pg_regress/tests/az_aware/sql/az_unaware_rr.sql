\connect 'host=localhost port=6432 user=az-unaware-user dbname=postgres'

-- With az_aware no, AZ tags on endpoints are ignored and round-robin
-- distributes queries evenly across all three hosts: 5432, 5433, 5434.

select 1;

select 2;

select 3;

select 4;

select 5;

select 6;
