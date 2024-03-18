#!/bin/bash

set -ex

cd /test_dir/test && /usr/bin/odyssey_test

setup

# gorm
ody-start
/gorm/test.sh
ody-stop

# proto
ody-start
/xproto/test.sh
ody-stop

# copy 
/copy/copy_test.sh
if [ $? -eq 1 ]
then
	exit 1
fi

# odyssey rule-address test
/rule-address/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi

# odyssey target session attrs test
/tsa/tsa.sh
if [ $? -eq 1 ]
then
	exit 1
fi

ody-start
/config-validation
ody-stop

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

# auth query
/auth_query/test_auth_query.sh
if [ $? -eq 1 ]
then
	exit 1
fi

# odyssey hba test
/hba/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi

#prepared statements in transaction pooling
/usr/bin/odyssey /etc/odyssey/pstmts.conf
sleep 1
/pstmts-test

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
/shell-test/pool_size_test.sh
ody-stop

ody-start
/ody-integration-test
ody-stop

teardown
