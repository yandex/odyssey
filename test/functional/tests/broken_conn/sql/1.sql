select 43;

set application_name to 'kill_me';

\! /tests/broken_conn/kill_server.sh

\! sleep 1

select 87;
