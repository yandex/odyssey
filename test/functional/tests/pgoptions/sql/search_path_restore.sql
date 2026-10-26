\set ON_ERROR_STOP on
\pset format unaligned
\pset tuples_only on
SELECT current_setting('search_path') = '"odyssey test path", "odyssey_test_""quote", public' AS startup_path;
CREATE SCHEMA "odyssey test path";
CREATE SCHEMA "odyssey_test_""quote";
SELECT current_schemas(false) = ARRAY['odyssey test path', 'odyssey_test_"quote', 'public']::name[] AS schema_order;
SELECT pg_backend_pid() AS saved_pid \gset
\! PGOPTIONS='-c search_path=pg_catalog' psql -XqAt -v ON_ERROR_STOP=1 -c "SELECT current_setting('search_path') = 'pg_catalog'"
\if :SHELL_ERROR
SELECT 1/0;
\endif
SELECT pg_backend_pid() = :saved_pid AS same_backend;
SELECT current_setting('search_path') = '"odyssey test path", "odyssey_test_""quote", public' AS restored_path;
SELECT current_schemas(false) = ARRAY['odyssey test path', 'odyssey_test_"quote', 'public']::name[] AS restored_order;
DROP SCHEMA "odyssey test path";
DROP SCHEMA "odyssey_test_""quote";
