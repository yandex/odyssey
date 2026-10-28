-- A fresh route to the primary first fails setup through the read-only listener.
\! psql 'host=127.0.0.1 port=7432 user=startup_params dbname=postgres' -c 'select 1' 2>&1

-- The next login must receive startup parameters, even after that TSA mismatch.
\connect 'host=127.0.0.1 port=8432 user=startup_params dbname=postgres client_encoding=UTF8'
SELECT :'SERVER_VERSION_NAME' <> '0.0.0' AS has_server_version,
       :'ENCODING' = 'UTF8' AS has_client_encoding;
