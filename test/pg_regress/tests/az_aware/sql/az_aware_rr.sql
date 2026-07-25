\connect 'host=localhost port=6432 user=az-aware-user dbname=postgres'

-- With az_aware yes and availability_zone "az1", only localhost:5432 (tagged :az1)
-- is in the local AZ.  Round-robin over a single local endpoint means every
-- query must be served by that host regardless of how many queries are sent.

select 1;

select 2;

select 3;

select 4;

select 5;

select 6;
