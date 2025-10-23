#!/bin/sh

set -ex

CONFIG_FILE="/etc/odyssey/odyssey.conf"

if [ ! -f "$CONFIG_FILE" ]; then
    cat > "$CONFIG_FILE" <<EOF
    unix_socket_dir "/tmp"
    unix_socket_mode "0644"

    log_format "%p %t %l [%i %s] (%c) %m\n"
    log_to_stdout yes
    log_session ${LOS_SESSION:-yes}
    log_query ${LOS_QUERY:-no}

    daemonize no

    locks_dir "/tmp/odyssey"

    listen {
        host ${LISTEN_HOST:-\"*\"}
        port ${LISTEN_PORT:-6432}
    }

    storage "local" {
        type "local"
    }

    storage "postgres_server" {
        type "remote"
        
        host ${PG_HOST:-\"127.0.0.1\"}
        port ${PG_PORT:-5432}
    }

    database ${DB_NAME:-default} {
        user ${USER_NAME:-default} {
            authentication ${USER_AUTH_TYPE:-\"clear_text\"}
		    password ${USER_PASSWORD:-\"password\"}
            storage "postgres_server"
            pool ${POOL_TYPE:-\"session\"}
            pool_size ${POOL_SIZE:-1} 

            client_fwd_error yes
        }
    }

    database ${VIRTUAL_DB_NAME:-\"console\"} {
        user ${VIRTUAL_DB_USER_NAME:-\"console\"} {
            authentication "none"
            role "admin"
            pool "session"
            storage "local"
        }
    }
EOF
fi

./odyssey $CONFIG_FILE
