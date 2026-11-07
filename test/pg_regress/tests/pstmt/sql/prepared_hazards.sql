\pset format unaligned
\set VERBOSITY sqlstate
select $1::int as v \parse x
-- Arguments are opaque SQL to the minimal parser.
SET odyssey.parser_mode = 'mini';
EXECUTE"x"(1+2);
SET odyssey.parser_mode = 'full';
EXECUTE"x"(1+2);
-- Unicode-escaped names require the full parser.
EXECUTE U&"\0078"(1+2);
SET odyssey.parser_mode = 'mini';
EXECUTE U&"\0078"(1+2);
-- Batched EXECUTE does not resolve protocol statement aliases.
SELECT 1 AS v \parse p1
SELECT 2 AS v \parse p2
EXECUTE p1;
EXECUTE p2;
EXECUTE p1 \; EXECUTE p2;
SET odyssey.parser_mode = 'full';
EXECUTE p1 \; EXECUTE p2;
SET odyssey.parser_mode = 'mini';
-- SQL names in Odyssey's namespace are reserved in either parser mode.
PREPARE odyssey AS SELECT 1;
PREPARE odyssey_ext AS SELECT 1 \parse reserved_sql
EXECUTE odyssey_pstmt_0(1+2);
DEALLOCATE odyssey_pstmt_0;
EXPLAIN EXECUTE odyssey_missing;
SET odyssey.parser_mode = 'full';
PREPARE "odyssey_custom" AS SELECT 1;
EXPLAIN EXECUTE odyssey_missing;
DEALLOCATE PREPARE "odyssey_custom" \parse reserved_deallocate
-- TODO: support reserved-name checks in query batches.
-- CREATE SEQUENCE reserved_batch;
-- SELECT nextval('reserved_batch') \; EXECUTE odyssey_pstmt_0;
-- SELECT is_called FROM reserved_batch;
-- DROP SEQUENCE reserved_batch;
-- The full parser checks decoded names, without a raw "odyssey" substring.
EXECUTE U&"\006fdyssey_missing";
EXECUTE U&"!006fdyssey_missing" UESCAPE '!' \parse reserved_unicode
-- A malformed EXECUTE must reach PostgreSQL without a reserved-name error.
EXECUTE odyssey_missing 6 + 7;
EXECUTE odyssey_missing 6 + 7 \parse invalid_execute
SET odyssey.parser_mode = 'mini';
-- A cached minimal-parser context must not bypass the full-parser check.
EXPLAIN EXECUTE odyssey_cache_missing \parse cached_mini
SET odyssey.parser_mode = 'full';
EXPLAIN EXECUTE odyssey_cache_missing \parse cached_full
SET odyssey.parser_mode = 'mini';
EXPLAIN EXECUTE odyssey_cache_missing \parse cached_mini_again
-- Protocol name "x" already exists: 42P05 must precede reserved SQL name 42939.
PREPARE odyssey_duplicate AS SELECT 1 \parse x
-- A literal or a case-sensitive quoted name is not reserved.
SELECT 'odyssey' AS v;
PREPARE "ODYSSEY" AS SELECT 1 AS v \; EXECUTE "ODYSSEY" \; DEALLOCATE "ODYSSEY";
