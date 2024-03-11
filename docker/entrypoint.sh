#!/bin/bash

set -ex

cd /test_dir/test && /usr/bin/odyssey_test

setup

# copy 
/copy/copy_test.sh
if [ $? -eq 1 ]
then
	exit 1
fi

teardown
