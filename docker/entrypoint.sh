#!/bin/bash

set -ex

cd /test_dir/test && /usr/bin/odyssey_test

setup

#ldap
/ldap/test_ldap.sh
if [ $? -eq 1 ] 
then
	exit 1
fi

# scram
/scram/test_scram.sh
if [ $? -eq 1 ] 
then
	exit 1
fi

ody-stop

# auth query
/auth_query/test_auth_query.sh
if [ $? -eq 1 ] 
then
	exit 1
fi

ody-stop

# lag polling 
/lagpolling/test-lag.sh
if [ $? -eq 1 ] 
then
	exit 1
fi

/usr/bin/odyssey-asan /etc/odyssey/odyssey.conf
ody-stop

# TODO: rewrite
#/shell-test/test.sh
/shell-test/console_role_test.sh
/shell-test/parse_pg_options_test.sh
/shell-test/override_pg_options_test.sh
ody-stop

ody-start
/ody-integration-test
ody-stop


teardown

