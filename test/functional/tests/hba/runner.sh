#!/bin/bash -x

set -ex

#
# TCP
#

/usr/bin/odyssey /tests/hba/tcp.conf
sleep 1

PGPASSWORD=correct_password psql -h ip4-localhost -p 6432 -U user_allow -c "SELECT 1" hba_db || {
  echo "ERROR: failed auth with hba trust, correct password and plain password in config"
	sleep 1
	cat /var/log/odyssey.log
	for i in /asan-output*; do
		cat $i
	done
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

PGPASSWORD=incorrect_password psql -h ip4-localhost -p 6432 -U user_allow -c "SELECT 1" hba_db && {
  echo "ERROR: successfully auth with hba trust, but incorrect password"
	sleep 1
	cat /var/log/odyssey.log
	for i in /asan-output*; do
		cat $i
	done
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

PGPASSWORD=correct_password psql -h ip4-localhost -p 6432 -U user_reject -c "SELECT 1" hba_db && {
  echo "ERROR: successfully auth with hba reject"

	cat /var/log/odyssey.log
	for i in /asan-output*; do
		cat $i
	done
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

PGPASSWORD=correct_password psql -h ip4-localhost -p 6432 -U user_unknown -c "SELECT 1" hba_db && {
  echo "ERROR: successfully auth without hba rule"
	sleep 1
	cat /var/log/odyssey.log
	for i in /asan-output*; do
		cat $i
	done
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}  

kill -s HUP $(pgrep odyssey)
sleep 1
PGPASSWORD=correct_password PGCONNECT_TIMEOUT=5 psql -h ip4-localhost -p 6432 -U user_allow -c "SELECT 1" hba_db || {
  echo "ERROR: unable to connect after SIGHUP"
	sleep 1
	cat /var/log/odyssey.log
	for i in /asan-output*; do
		cat $i
	done
	echo "
	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

ody-stop

#
# Unix
#

echo "" > /var/log/odyssey.log
/usr/bin/odyssey /tests/hba/unix.conf
sleep 1

PGPASSWORD=correct_password psql -h /tmp -p 6432 -U user_allow -c "SELECT 1" hba_db || {
    echo "ERROR: failed auth with hba trust, correct password and plain password in config"
	sleep 1
	cat /var/log/odyssey.log
	for i in /asan-output*; do
		cat $i
	done
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

PGPASSWORD=correct_password psql -h /tmp -p 6432 -U user_reject -c "SELECT 1" hba_db && {
  echo "ERROR: successfully auth with hba reject"
	sleep 1
	cat /var/log/odyssey.log
	for i in /asan-output*; do
		cat $i
	done
	echo "

	"
	cat /var/log/postgresql/postgresql-16-main.log

	exit 1
}

ody-stop
