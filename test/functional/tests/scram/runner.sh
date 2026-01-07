#!/bin/bash -x

/usr/bin/odyssey /tests/scram/config.conf
sleep 3

/tests/scram/test_scram_backend.sh
if [ $? -eq 1 ]
then
	exit 1
fi
/tests/scram/test_scram_frontend.sh
if [ $? -eq 1 ]
then
	exit 1
fi

ody-stop
