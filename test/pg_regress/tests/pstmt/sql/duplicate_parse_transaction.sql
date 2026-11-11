\pset format unaligned
\set VERBOSITY sqlstate
CREATE TABLE duplicate_parse_tx (v int);
SELECT 1 AS v \parse duplicate_parse

-- A duplicate Parse aborts the transaction and rolls back previous writes.
BEGIN;
INSERT INTO duplicate_parse_tx VALUES (1);
SELECT 2 AS v \parse duplicate_parse
SELECT 3;
-- A cached Parse must not succeed while the transaction is aborted.
SELECT 1 AS v \parse duplicate_after_error
COMMIT;
SELECT count(*) AS rows_after_commit FROM duplicate_parse_tx;
EXECUTE duplicate_parse;
EXECUTE duplicate_after_error;

-- A rollback to savepoint recovers the transaction and the original Parse.
BEGIN;
SAVEPOINT before_duplicate;
SELECT 2 AS v \parse duplicate_parse
SELECT 3;
ROLLBACK TO before_duplicate;
INSERT INTO duplicate_parse_tx VALUES (2);
COMMIT;
SELECT v FROM duplicate_parse_tx;
EXECUTE duplicate_parse;

-- A pipelined duplicate discards later messages until Sync.
BEGIN;
\startpipeline
SELECT 9 AS v \parse duplicate_parse
SELECT 10 AS v \parse discarded_pipeline
\syncpipeline
\getresults
\endpipeline
SELECT 3;
ROLLBACK;
EXECUTE discarded_pipeline;

-- An earlier backend error wins over a later virtual duplicate error.
BEGIN;
\startpipeline
SELECT * FROM duplicate_parse_missing_table \parse failed_pipeline
SELECT 9 AS v \parse duplicate_parse
\syncpipeline
\getresults
\endpipeline
SELECT 3;
ROLLBACK;

-- A prior failing Bind also takes precedence over the duplicate Parse.
BEGIN;
\startpipeline
\bind_named duplicate_parse 1
\sendpipeline
SELECT 9 AS v \parse duplicate_parse
\syncpipeline
\getresults
\endpipeline
SELECT 3;
ROLLBACK;

-- Flush does not end the error state; Sync does.
BEGIN;
\startpipeline
SELECT 9 AS v \parse duplicate_parse
\flushrequest
\getresults
SELECT 10 AS v \parse discarded_flush
\syncpipeline
\getresults
\endpipeline
SELECT 3;
ROLLBACK;
EXECUTE discarded_flush;

-- The duplicate can be the first command after a deferred BEGIN.
BEGIN;
SELECT 9 AS v \parse duplicate_parse
SELECT 3;
ROLLBACK;

-- Other virtual Parse errors must abort the server transaction too.
BEGIN;
PREPARE odyssey_reserved AS SELECT 1 \parse reserved_parse
SELECT 3;
ROLLBACK;

-- Outside a transaction Sync restores normal operation.
SELECT 2 AS v \parse duplicate_parse
SELECT 4 AS v;
\close_prepared duplicate_parse
DROP TABLE duplicate_parse_tx;
