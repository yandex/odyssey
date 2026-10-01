-- tsa is still checked against the pinned host: primary + read-only has no
-- suitable host, so the attach is rejected with FATAL and the connection is
-- closed; run it in a child psql, so that the test session itself stays alive
\! psql 'host=localhost port=6432 user=postgres dbname=postgres' -c "SET odyssey.execute_on_host TO 'localhost:5432'" -c "SET odyssey.target_session_attrs TO 'read-only'" -c 'select pg_is_in_recovery()' 2>&1

-- same host without tsa restriction is fine
\! psql 'host=localhost port=6432 user=postgres dbname=postgres' -c "SET odyssey.execute_on_host TO 'localhost:5432'" -c 'select pg_is_in_recovery()' 2>&1
