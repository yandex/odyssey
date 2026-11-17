-- A rejected deferred BEGIN must not disconnect the client or run its query.
\pset format unaligned
SET odyssey.execute_on_host TO 'localhost:5433';
SET odyssey.parser_mode = 'full';
BEGIN READ WRITE;
SELECT 42;
SELECT 43;
BEGIN ISOLATION LEVEL SERIALIZABLE;
SELECT 44;
SELECT 45;
