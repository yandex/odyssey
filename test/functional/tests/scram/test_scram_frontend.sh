#!/bin/bash -x

for _ in $(seq 1 5); do
        psql 'host=localhost port=6432 user=frontend_auth_plain dbname=scram_db password=incorrect_password sslmode=disable' -c "SELECT 1" 2>&1 && {
                echo "ERROR: successfully auth with incorrect password and plain password in config"
                ody-stop
                exit 1
        }
done

for _ in $(seq 1 5); do
        psql 'host=localhost port=6432 user=frontend_auth_plain dbname=scram_db password=correct_password sslmode=disable' -c "SELECT 1" 2>&1 || {
                echo "ERROR: failed auth with correct password and plain password in config"
                ody-stop
                exit 1
        }
done

for _ in $(seq 1 5); do
        psql 'host=localhost port=6432 user=frontend_auth_scram_secret dbname=scram_db password=incorrect_password sslmode=disable' -c "SELECT 1" 2>&1 && {
                echo "ERROR: successfully auth with incorrect password and scram secret in config"
                ody-stop
                exit 1
        }
done

for _ in $(seq 1 5); do
        psql 'host=localhost port=6432 user=frontend_auth_scram_secret dbname=scram_db password=correct_password sslmode=disable' -c "SELECT 1" 2>&1 || {
                echo "ERROR: failed auth with correct password and scram secret in config"
                ody-stop
                exit 1
        }
done
