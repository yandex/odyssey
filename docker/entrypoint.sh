#!/bin/bash

set -ex

setup

# odyssey rule-address test
/rule-address/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi

#prepared statements in transaction pooling
/usr/bin/odyssey /etc/odyssey/pstmts.conf
sleep 1
/pstmts-test

ody-stop

teardown
