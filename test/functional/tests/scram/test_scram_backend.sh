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

for _ in $(seq 1 5); do
        PGPASSWORD=scram_user_password psql 'host=ip4-localhost port=6432 user=frontend_and_backend_auth_scram_password dbname=scram_db sslmode=disable' -c "SELECT 1" 2>&1 || {
                echo "ERROR: failed frontend+backend auth with scram password"

                cat /var/log/odyssey.log
                echo "


                "
                cat /var/log/postgresql/postgresql-16-main.log

                exit 1
        }
done

for _ in $(seq 1 5); do
        PGPASSWORD=scram_user_password_incorrect psql 'host=ip4-localhost port=6432 user=frontend_and_backend_auth_scram_password dbname=scram_db sslmode=disable' -c "SELECT 1" 2>&1 && {
                echo "ERROR: successfully backend auth with incorrect password"

                sleep 1
                cat /var/log/odyssey.log
                echo "


                "
                cat /var/log/postgresql/postgresql-16-main.log

                exit 1
        }
done

original_verifier=$(psql -h ip4-localhost -U postgres -d scram_db -At \
        -v ON_ERROR_STOP=1 -c "SELECT rolpassword FROM pg_authid WHERE rolname = 'scram_user'") || exit 1

restore_password() {
        psql -h ip4-localhost -U postgres -d scram_db -v ON_ERROR_STOP=1 \
                -v verifier="$original_verifier" <<'SQL' || exit 1
ALTER ROLE scram_user PASSWORD :'verifier';
SQL
}
trap restore_password EXIT

for salt_len in 8 32; do
        echo "Testing backend SCRAM with salt length $salt_len"
        verifier=$(python3 - "$salt_len" <<'PY'
import base64
import hashlib
import hmac
import sys

salt = bytes(range(int(sys.argv[1])))
salted_password = hashlib.pbkdf2_hmac('sha256', b'scram_user_password', salt, 4096)
client_key = hmac.digest(salted_password, b'Client Key', 'sha256')
stored_key = hashlib.sha256(client_key).digest()
server_key = hmac.digest(salted_password, b'Server Key', 'sha256')
salt, stored_key, server_key = (
    base64.b64encode(value).decode() for value in (salt, stored_key, server_key)
)
print(f'SCRAM-SHA-256$4096:{salt}${stored_key}:{server_key}')
PY
        ) || exit 1
        psql -h ip4-localhost -U postgres -d scram_db -v ON_ERROR_STOP=1 \
                -v verifier="$verifier" <<'SQL' || exit 1
ALTER ROLE scram_user PASSWORD :'verifier';
SQL

        # Drop pooled connections so the new verifier is used for authentication.
        ody-stop || exit 1
        /usr/bin/odyssey /tests/scram/config.conf || exit 1
        for _ in $(seq 1 50); do
                pg_isready -q -h ip4-localhost -p 6432 -d scram_db \
                        -U backend_auth_with_correct_password && break
                sleep 0.1
        done

        PGPASSWORD=scram_user_password psql -w -v ON_ERROR_STOP=1 \
                'host=ip4-localhost port=5432 user=scram_user dbname=scram_db sslmode=disable' -c "SELECT 1" || exit 1

        for _ in $(seq 1 5); do
                psql -w -v ON_ERROR_STOP=1 \
                        'host=ip4-localhost port=6432 user=backend_auth_with_correct_password dbname=scram_db sslmode=disable' -c "SELECT 1" 2>&1 || {
                        echo "ERROR: failed backend auth with salt length $salt_len"
                        cat /var/log/odyssey.log
                        exit 1
                }
        done

        psql -w -v ON_ERROR_STOP=1 \
                'host=ip4-localhost port=6432 user=backend_auth_with_incorrect_password dbname=scram_db sslmode=disable' -c "SELECT 1" 2>&1 && {
                echo "ERROR: accepted incorrect password with salt length $salt_len"
                cat /var/log/odyssey.log
                exit 1
        }
done

exit 0
