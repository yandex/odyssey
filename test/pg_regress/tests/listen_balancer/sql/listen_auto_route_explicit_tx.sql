\connect 'host=localhost port=6432 user=autotxuser dbname=postgres'
\pset format unaligned

-- Explicit transactions must stay on a primary even when they only read.
BEGIN;
SELECT 1;
SELECT 2;
SELECT pg_is_in_recovery();
COMMIT;

-- Reads outside an explicit transaction still prefer a standby.
SELECT pg_is_in_recovery();

CREATE TABLE auto_route_explicit_tx (i integer);

-- A read does not imply that the rest of the transaction will be read-only.
BEGIN;
SELECT 1;
SELECT pg_is_in_recovery();
INSERT INTO auto_route_explicit_tx VALUES (42);
SELECT i FROM auto_route_explicit_tx;
COMMIT;

-- The same transaction can use extended protocol messages after BEGIN.
BEGIN;
SELECT 1 \bind
\g
SELECT pg_is_in_recovery() \bind
\g
INSERT INTO auto_route_explicit_tx VALUES (43) \bind
\g
COMMIT;

BEGIN;
SELECT i FROM auto_route_explicit_tx ORDER BY i;
DROP TABLE auto_route_explicit_tx;
COMMIT;

SELECT pg_is_in_recovery();
