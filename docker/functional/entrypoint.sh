#!/bin/bash

set -ex

/usr/bin/start-pg

# path to log file
/tests/invalid_log_file/test_log_file.sh
if [ $? -eq 1 ]
then
	exit 1
fi

/tests/tls-compat/test-tls-compat.sh
if [ $? -eq 1 ]
then
	exit 1
fi

/tests/reload/test-reload.sh
if [ $? -eq 1 ]
then
	exit 1
fi

# group
/tests/group/test_group.sh
if [ $? -eq 1 ]
then
	exit 1
fi

echo "" > /var/log/odyssey.log
# gorm
/tests/gorm/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# proto
/tests/xproto/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# copy 
/tests/copy/copy_test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# odyssey rule_address test
/tests/rule_address/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi

echo "" > /var/log/odyssey.log

/tests/tsa_ports/test-tsa-ports.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# odyssey target session attrs test
/tests/tsa/tsa.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

/tests/config-validation/runner.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

#ldap
/tests/ldap/test_ldap.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# scram
/tests/scram/test_scram.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# auth query
/tests/auth_query/test_auth_query.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# odyssey hba test
/tests/hba/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

#prepared statements in transaction pooling
/tests/prep_stmts/runner.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# lag polling
/tests/lagpolling/test-lag.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# pause-resume
echo "" > /var/log/odyssey.log
/tests/pause-resume/test-pause-resume.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

/tests/shell-test/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

/tests/ody_integration_test/runner.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

/tests/npgsql_compat/test_npgsql_compat.sh
if [ $? -eq 1 ]
then
	exit 1
fi
