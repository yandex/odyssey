#!/bin/bash
set -ex

cd /test_dir/test && /usr/bin/odyssey_test

setup

#ldap
/ldap/test_ldap.sh

# scram
#/scram/test_scram.sh

ody-start
/ody-integration-test
ody-stop

/usr/bin/odyssey-asan /etc/odyssey/odyssey.conf

## /shell-test/test.sh

teardown
