CREATE USER suser WITH PASSWORD 'postgres';
GRANT ALL ON DATABASE postgres TO suser;
GRANT ALL ON SCHEMA public TO suser;

CREATE USER tuser WITH PASSWORD 'postgres';
GRANT ALL ON DATABASE postgres TO tuser;
GRANT ALL ON SCHEMA public TO tuser;

CREATE ROLE replicator WITH REPLICATION LOGIN PASSWORD 'postgres';
SELECT pg_create_physical_replication_slot('replication_slot');
