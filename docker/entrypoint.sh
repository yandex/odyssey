#!/bin/bash
set -ex

setup

#ldap
/ldap/test_ldap.sh

# scram
/scram/test_scram.sh

ody-start
/ody-intergration-test
ody-stop

/usr/bin/odyssey-asan /etc/odyssey/odyssey.conf

## /shell-test/test.sh

teardown
