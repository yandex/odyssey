\set ON_ERROR_STOP on
\pset format unaligned
\pset tuples_only on
SELECT length(current_setting('search_path')) > 512 AS long_path;
SELECT current_schemas(false) = ARRAY['public']::name[] AS schema_order;
\! PGOPTIONS='-c search_path=pg_catalog' psql -XqAt -v ON_ERROR_STOP=1 -c "SELECT current_setting('search_path') = 'pg_catalog'"
\if :SHELL_ERROR
SELECT 1/0;
\endif
SELECT length(current_setting('search_path')) > 512 AS restored_path;
SELECT current_schemas(false) = ARRAY['public']::name[] AS restored_order;
