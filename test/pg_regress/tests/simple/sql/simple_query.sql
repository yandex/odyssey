\pset format unaligned
-- One Query may contain several results and still has just one ReadyForQuery.
SELECT 1 AS first \; SELECT 2 AS second;
SELECT 3 AS before_error \; SELECT 1 / 0 \; SELECT 4 AS skipped;
SELECT 5 AS after_error;

-- An error in a real transaction must preserve the failed transaction state.
BEGIN;
SELECT 6 AS in_transaction;
SELECT 1 / 0;
SELECT 7 AS aborted;
ROLLBACK;
SELECT 8 AS after_rollback;

-- COPY may stop streaming before ReadyForQuery and resume in the relay.
CREATE TABLE simple_query_copy (v int);
COPY simple_query_copy FROM STDIN;
9
10
\.
COPY (SELECT v FROM simple_query_copy ORDER BY v) TO STDOUT;
DROP TABLE simple_query_copy;
