#!/bin/bash -x

set -ex

users=("user1" "user2" "user3" "user4" "user5")
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

psql -h localhost -p 6432 -U checker1 -c "GRANT group1 TO user1;" group_db
sleep(1000)
psql -h localhost -p 6432 -U user1 -c "SELECT 1" group_db >/dev/null 2>&1 || {
	echo "ERROR: group auth apply for over user at config"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

psql -h localhost -p 6432 -U checker1 -c "GRANT group1 TO user2;" group_db
sleep(1000)
psql -h localhost -p 6432 -U user2 -c "SELECT 1" group_db >/dev/null 2>&1 && {
	echo "ERROR: group auth not apply"

	cat /var/log/odyssey.log
	echo "

	"
	cat /var/log/postgresql/postgresql-14-main.log

	exit 1
}

# TODO: еще тесты дописать

ody-stop
