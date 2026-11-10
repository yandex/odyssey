\pset format unaligned
SET odyssey.parser_mode = 'mini';
\getenv test_dir PGTEST_DIR
\i :test_dir/sql/sql_prepare_body.sql
