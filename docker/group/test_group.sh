#!/bin/bash -x

set -ex

/usr/bin/odyssey /group/config.conf

users=("group_user1" "group_user2" "group_user3" "group_user4" "group_user5")
for user in "${users[@]}"; do
	psql -h localhost -p 6432 -U "$user" -c "SELECT 1" group_db >/dev/null 2>&1 && {
		echo "ERROR: Authenticated with non-grouped user"

		cat /var/log/odyssey.log
		echo "

		"
		cat /var/log/postgresql/postgresql-14-main.log

		exit 1
	}
done

ody-stop

psql -h localhost -p 5432 -U postgres -c "GRANT group1 TO group_user1;" group_db
psql -h localhost -p 5432 -U postgres -c "GRANT group1 TO group_user2;" group_db
psql -h localhost -p 5432 -U postgres -c "GRANT group2 TO group_user3;" group_db
psql -h localhost -p 5432 -U postgres -c "GRANT group2 TO group_user4;" group_db

/usr/bin/odyssey /group/config.conf

sleep 3

psql -h localhost -p 6432 -U group_user1 -c "SELECT 1" group_db >/dev/null 2>&1 && {
	echo "ERROR: Authenticated without password"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

PGPASSWORD=password1 psql -h localhost -p 6432 -U group_user1 -c "SELECT 1" group_db >/dev/null 2>&1 || {
	echo "ERROR: Not authenticated with correct password"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

psql -h localhost -p 6432 -U group_user3 -c "SELECT 1" group_db >/dev/null 2>&1 || {
	echo "ERROR: Not authenticated with disabled auth"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

ody-stop
