#!/bin/bash

set -ex

setup

# odyssey hba test
/hba/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi

teardown
