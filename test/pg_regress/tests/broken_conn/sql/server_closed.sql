select 43;

set application_name to 'kill_me';

\! ${PGTEST_DIR}/sql/kill_server.sh

\! sleep 1

select 87;
