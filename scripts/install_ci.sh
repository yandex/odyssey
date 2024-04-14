#!/usr/bin/env bash

set -e

if ! sudo sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'; then
    echo "Error adding PostgreSQL repository."
    exit 1
fi

if ! wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -; then
    echo "Error adding PostgreSQL repository key."
    exit 1
fi

if ! sudo apt-get update; then
    echo "Error updating package list."
    exit 1
fi

if ! sudo apt-get -y --no-install-recommends install postgresql-14 postgresql-server-dev-14 libpq5 libpq-dev clang-format-11 libpam0g-dev libldap-dev; then
    echo "Error installing PostgreSQL and its dependencies."
    exit 1
fi

if pgrep "postgres" > /dev/null; then
    if ! sudo pkill -9 postgres; then
        echo "Error stopping PostgreSQL process."
        exit 1
    fi
fi

if ! sudo sh -c 'echo -n | openssl s_client -connect https://scan.coverity.com:443 | sed -ne "/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p" >> /etc/ssl/certs/ca-certificates.crt'; then
    echo "Error adding SSL certificate."
    exit 1
fi

if ! sudo apt-get clean; then
    echo "Error cleaning apt-get cache."
    exit 1
fi

echo "Script completed successfully."
exit 0