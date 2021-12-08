#!/bin/bash

set -ex

cd /test_dir/test && /usr/bin/odyssey_test

setup

#ldap
/ldap/test_ldap.sh
if [ $?  -eq 1] 
then
	exit 1
fi

# scram
/scram/test_scram.sh

ody-stop

# lag polling 
/shell-test/test-lag.sh
if [ $?  -eq 1] 
then
	exit 1
fi

ody-start
/ody-integration-test
ody-stop

/usr/bin/odyssey-asan /etc/odyssey/odyssey.conf

/shell-test/test.sh

teardown
