\connect 'host=localhost port=6432 user=txuser dbname=postgres'
\pset format unaligned

-- The deferred BEGIN and the SELECT arrive as separate Query messages.
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
SELECT 1;
SHOW transaction_isolation;
SELECT pg_is_in_recovery();
COMMIT;

-- READ ONLY does not make SERIALIZABLE available on a hot standby.
BEGIN ISOLATION LEVEL SERIALIZABLE, READ ONLY, DEFERRABLE;
SELECT 1;
SHOW transaction_isolation;
SHOW transaction_read_only;
SELECT pg_is_in_recovery();
COMMIT;

-- An explicit READ WRITE also requires a primary, even for a SELECT.
BEGIN TRANSACTION READ WRITE;
SELECT 1;
SHOW transaction_read_only;
SELECT pg_is_in_recovery();
ROLLBACK;

-- The first statement after BEGIN can use the extended protocol too.
BEGIN ISOLATION LEVEL SERIALIZABLE;
SELECT pg_is_in_recovery() \bind
\g
SHOW transaction_isolation;
COMMIT;

-- Send the whole batch in one Query, rather than splitting at semicolons.
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE \; SELECT 1 \; SHOW transaction_isolation \; SELECT pg_is_in_recovery() \g
ROLLBACK;

-- Safe transaction options still allow the first SELECT to choose a standby.
BEGIN ISOLATION LEVEL REPEATABLE READ READ ONLY;
SELECT pg_is_in_recovery();
SHOW transaction_isolation;
SHOW transaction_read_only;
COMMIT;

BEGIN;
SELECT pg_is_in_recovery();
COMMIT;

-- The primary requirement must not leak into the next transaction.
SELECT pg_is_in_recovery();
