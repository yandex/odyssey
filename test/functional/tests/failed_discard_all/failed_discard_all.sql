SELECT 42 AS v \parse p1
BEGIN;
SELECT 1;
-- fails: DISCARD ALL cannot run inside a transaction block
DISCARD ALL;
ROLLBACK;
-- the statement must still exist
\bind_named p1 \g
