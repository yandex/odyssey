#!/bin/bash -x

/usr/bin/odyssey /scram/config.conf

/scram/test_scram_backend.sh
if [ $? -eq 1 ]
then
	exit 1
fi
/scram/test_scram_frontend.sh
if [ $? -eq 1 ]
then
	exit 1
fi

ody-stop
