\set ON_ERROR_STOP on
\pset format unaligned
\pset tuples_only on
CREATE SCHEMA "user";
SELECT current_schemas(false) = ARRAY['user']::name[] AS keyword_schema;
\! PGOPTIONS='-c search_path=public' psql -XqAt -v ON_ERROR_STOP=1 -c "SELECT current_setting('search_path') = 'public'"
\if :SHELL_ERROR
SELECT 1/0;
\endif
SELECT current_schemas(false) = ARRAY['user']::name[] AS restored_schema;
DROP SCHEMA "user";
