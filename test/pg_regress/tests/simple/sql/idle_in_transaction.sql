\! psql --echo-all --no-psqlrc --quiet -p ${PGTEST_PORT} -h localhost -U ${PGTEST_USER} -d ${PGTEST_DB} -f ${PGTEST_DIR}/sql/idle_in_transaction_body.sql 2>&1; true
