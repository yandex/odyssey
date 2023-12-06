#!/bin/bash

set -ex

setup

# odyssey rule-address test
/rule-address/test.sh
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

teardown
