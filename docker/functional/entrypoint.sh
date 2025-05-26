#!/bin/bash

set -ex

/usr/bin/odyssey /etc/odyssey/odyssey.conf
sleep 5
ody-stop

/usr/bin/start-pg

# path to log file
/invalid_log_file/test_log_file.sh
if [ $? -eq 1 ]
then
	exit 1
fi

/tls-compat/test-tls-compat.sh
if [ $? -eq 1 ]
then
	exit 1
fi

/reload/test-reload.sh
if [ $? -eq 1 ]
then
	exit 1
fi

# group
/group/test_group.sh
if [ $? -eq 1 ]
then
	exit 1
fi

echo "" > /var/log/odyssey.log
# gorm
ody-start
/gorm/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
ody-stop
echo "" > /var/log/odyssey.log

# proto
ody-start
/xproto/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
ody-stop
echo "" > /var/log/odyssey.log

# copy 
/copy/copy_test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# odyssey rule_address test
/rule_address/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi

echo "" > /var/log/odyssey.log

# odyssey target session attrs test
/tsa/tsa.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

ody-start
/config-validation
ody-stop
echo "" > /var/log/odyssey.log

#ldap
/ldap/test_ldap.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# scram
/scram/test_scram.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# auth query
/auth_query/test_auth_query.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# odyssey hba test
/hba/test.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

#prepared statements in transaction pooling
/usr/bin/odyssey /etc/odyssey/pstmts.conf
sleep 1
/pstmts-test

ody-stop
echo "" > /var/log/odyssey.log

# lag polling
/lagpolling/test-lag.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# pause-resume
echo "" > /var/log/odyssey.log
/pause-resume/test-pause-resume.sh
if [ $? -eq 1 ]
then
	exit 1
fi
echo "" > /var/log/odyssey.log

# TODO: rewrite
#/shell-test/test.sh
/shell-test/console_role_test.sh
/shell-test/parse_pg_options_test.sh
/shell-test/override_pg_options_test.sh
/shell-test/pool_size_test.sh
ody-stop

ody-start
/ody_integration_test
ody-stop

/npgsql_compat/test_npgsql_compat.sh
if [ $? -eq 1 ]
then
	exit 1
fi
