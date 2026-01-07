#!/bin/bash -x


for _ in $(seq 1 5); do
        psql 'host=ip4-localhost port=6432 user=backend_auth_with_incorrect_password dbname=scram_db sslmode=disable' -c "SELECT 1" 2>&1 && {
                echo "ERROR: successfully backend auth with incorrect password"

                cat /var/log/odyssey.log
                echo "


                "
                cat /var/log/postgresql/postgresql-16-main.log

                exit 1
        }
done

for _ in $(seq 1 5); do
        psql 'host=ip4-localhost port=6432 user=backend_auth_with_correct_password dbname=scram_db sslmode=disable' -c "SELECT 1" 2>&1 || {
                echo "ERROR: failed backend auth with correct password"

                cat /var/log/odyssey.log
                echo "


                "
                cat /var/log/postgresql/postgresql-16-main.log

                exit 1
        }
done
