SELECT 'z';

SET application_name to 'AAA';
SHOW application_name;

SET application_name to 'This-is-long-application-name-that-must-be-cut-to-add-host-and-this-is-tail';
SHOW application_name;

-- SET immediately after a virtual BEGIN must stay inside the transaction.
\pset format unaligned
SET application_name = 'before_tx';
BEGIN;
SET application_name = 'xx';
SHOW application_name;
ROLLBACK;
SHOW application_name;

-- Rewritten SET and EXECUTE share the pending query and clear it after sending.
SELECT current_setting('application_name') AS application_name \parse appname_stmt
BEGIN;
EXECUTE appname_stmt;
ROLLBACK;
BEGIN;
SET application_name = 'exec_tx';
EXECUTE appname_stmt;
ROLLBACK;
EXECUTE appname_stmt;
\close_prepared appname_stmt
