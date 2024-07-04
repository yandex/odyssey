#!/bin/bash -x

set -ex

/usr/bin/odyssey /rule-address/addr.conf

PGPASSWORD=correct_password psql -h 127.0.0.1 -p 6432 -U user_addr_correct -c "SELECT 1" addr_db > /dev/null 2>&1 || {
  echo "ERROR: failed auth with correct addr, correct password and plain password in config"

	cat /var/log/odyssey.log

	exit 1
}

PGPASSWORD=incorrect_password psql -h 127.0.0.1 -p 6432 -U user_addr_correct -c "SELECT 1" addr_db > /dev/null 2>&1 && {
  echo "ERROR: successfully auth with correct addr, but incorrect password"

	cat /var/log/odyssey.log

	exit 1
}

PGPASSWORD=correct_password psql -h 127.0.0.1 -p 6432 -U user_addr_incorrect -c "SELECT 1" addr_db > /dev/null 2>&1 && {
  echo "ERROR: successfully auth with incorrect addr"

	cat /var/log/odyssey.log

	exit 1
}

PGPASSWORD=correct_password psql -h 127.0.0.1 -p 6432 -U user_addr_default -c "SELECT 1" addr_db > /dev/null 2>&1 || {
  echo "ERROR: failed auth with correct addr, correct password and plain password in config"

	cat /var/log/odyssey.log

	exit 1
}

PGPASSWORD=incorrect_password psql -h 127.0.0.1 -p 6432 -U user_addr_default -c "SELECT 1" addr_db > /dev/null 2>&1 && {
  echo "ERROR: successfully auth with correct addr, but incorrect password"

	cat /var/log/odyssey.log

	exit 1
}

PGPASSWORD=correct_password psql -h 127.0.0.1 -p 6432 -U user_addr_empty -c "SELECT 1" addr_db > /dev/null 2>&1 || {
  echo "ERROR: failed auth with correct addr, correct password and plain password in config"

	cat /var/log/odyssey.log

	exit 1
}

PGPASSWORD=incorrect_password psql -h 127.0.0.1 -p 6432 -U user_addr_empty -c "SELECT 1" addr_db > /dev/null 2>&1 && {
  echo "ERROR: successfully auth with correct addr, but incorrect password"

	cat /var/log/odyssey.log

	exit 1
}

PGPASSWORD=correct_password psql -h 127.0.0.1 -p 6432 -U user_addr_hostname_localhost -c "SELECT 1" addr_db > /dev/null 2>&1 || {
  echo "ERROR: failed auth with correct addr, correct password and plain password in config"

	cat /var/log/odyssey.log

	exit 1
}

PGPASSWORD=incorrect_password psql -h 127.0.0.1 -p 6432 -U user_addr_hostname_localhost -c "SELECT 1" addr_db > /dev/null 2>&1 && {
  echo "ERROR: successfully auth with correct addr, but incorrect password"

	cat /var/log/odyssey.log

	exit 1
}

ody-stop
