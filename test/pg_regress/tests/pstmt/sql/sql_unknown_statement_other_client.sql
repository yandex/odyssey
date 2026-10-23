-- run by sql_unknown_statement.sql as another client of the same route:
-- dynamic SQL creates a statement odyssey can not see
do $$ begin execute 'prepare sql_taken as select ''hidden'' as v'; end $$;
