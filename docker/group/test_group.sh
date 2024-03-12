#!/bin/bash -x

set -ex

/usr/bin/odyssey /group/config.conf

users=("group_user1" "group_user2" "group_user3" "group_user4" "group_user5")
for user in "${users[@]}"; do
	psql -h localhost -p 6432 -U "$user" -c "SELECT 1" group_db >/dev/null 2>&1 || {
		echo "ERROR: failed backend auth with correct user auth"

		cat /var/log/odyssey.log
		echo "

		"
		cat /var/log/postgresql/postgresql-14-main.log

		exit 1
	}
done

psql -h localhost -p 6432 -U postgres -c "GRANT group1 TO group_user1;" group_db
sleep 1
psql -h localhost -p 6432 -U group_user1 -c "SELECT 1" group_db >/dev/null 2>&1 || {
	echo "ERROR: group auth apply for over user at config"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

psql -h localhost -p 6432 -U postgres -c "GRANT group1 TO group_user2;" group_db
sleep 1
psql -h localhost -p 6432 -U group_user2 -c "SELECT 1" group_db >/dev/null 2>&1 && {
	echo "ERROR: group auth not apply"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

psql -h localhost -p 6432 -U postgres -c "GRANT group1 TO group_user4;" group_db
psql -h localhost -p 6432 -U postgres -c "GRANT group2 TO group_user4;" group_db
sleep 1
PGPASSWORD=password1 psql -h localhost -p 6432 -U group_user4 -c "SELECT 1" group_db >/dev/null 2>&1 && {
	echo "ERROR: group auth not accepted down group"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}
PGPASSWORD=password2 psql -h localhost -p 6432 -U group_user4 -c "SELECT 1" group_db >/dev/null 2>&1 || {
	echo "ERROR: group auth not apply"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

ody-stop
