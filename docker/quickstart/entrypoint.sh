#!/bin/sh

set -e

CONFIG_FILE="/etc/odyssey/odyssey.conf"

if [ ! -f "$CONFIG_FILE" ]; then
    DB_VALUE=${DB_NAME:+\"${DB_NAME}\"}
    DB_VALUE=${DB_VALUE:-default}

    USER_VALUE=${USER_NAME:+\"${USER_NAME}\"}
    USER_VALUE=${USER_VALUE:-default}

    VIRTUAL_DB_VALUE=${VIRTUAL_DB_NAME:+\"${VIRTUAL_DB_NAME}\"}
    VIRTUAL_DB_VALUE=${VIRTUAL_DB_VALUE:-\"console\"}

    VIRTUAL_USER_VALUE=${VIRTUAL_DB_USER_NAME:+\"${VIRTUAL_DB_USER_NAME}\"}
    VIRTUAL_USER_VALUE=${VIRTUAL_USER_VALUE:-\"console\"}

    cat > "$CONFIG_FILE" <<EOF
${PRE_INCLUDE_PATH:+include \"${PRE_INCLUDE_PATH}\"}

daemonize ${DAEMONIZE:-no}
sequential_routing ${SEQUENTIAL_ROUTING:-no}
log_to_stdout ${LOG_TO_STDOUT:-yes}
log_format "${LOG_FORMAT:-%p %t %l [%i %s] (%c) %m}"
log_debug ${LOG_DEBUG:-no}
log_config ${LOG_CONFIG:-no}
log_session ${LOG_SESSION:-yes}
log_query ${LOG_QUERY:-no}
log_stats ${LOG_STATS:-yes}
stats_interval ${STATS_INTERVAL:-10}
workers ${WORKERS:-\"auto\"}
resolvers ${RESOLVERS:-1}
readahead ${READAHEAD:-4096}
cache_coroutine ${CACHE_COROUTINE:-1024}
nodelay ${NODELAY:-yes}
keepalive ${KEEPALIVE:-15}
keepalive_keep_interval ${KEEPALIVE_KEEP_INTERVAL:-5}
keepalive_probes ${KEEPALIVE_PROBES:-3}
keepalive_usr_timeout ${KEEPALIVE_USR_TIMEOUT:-0}
coroutine_stack_size ${COROUTINE_STACK_SIZE:-8}
client_max ${CLIENT_MAX:-20000}
client_max_routing ${CLIENT_MAX_ROUTING:-0}
server_login_retry ${SERVER_LOGIN_RETRY:-1}
${HBA_FILE:+hba_file ${HBA_FILE}}
graceful_die_on_errors ${GRACEFUL_DIE_ON_ERRORS:-no}
graceful_shutdown_timeout_ms ${GRACEFUL_SHUTDOWN_TIMEOUT_MS:-30000}
${AVAILABILITY_ZONE:+availability_zone \"${AVAILABILITY_ZONE}\"}
enable_online_restart ${ENABLE_ONLINE_RESTART:-yes}
bindwith_reuseport ${BINDWITH_REUSEPORT:-yes}
max_sigterms_to_die ${MAX_SIGTERMS_TO_DIE:-3}
enable_host_watcher ${ENABLE_HOST_WATCHER:-no}

listen {
    host                    "${LISTEN_HOST:-0.0.0.0}"
    port                    ${LISTEN_PORT:-6432}
    backlog                 ${BACKLOG:-16}
    tls                     "${LISTEN_TLS_MODE:-disable}"
    tls_cert_file           "${LISTEN_TLS_CERT_FILE}"
    tls_key_file            "${LISTEN_TLS_KEY_FILE}"
    tls_ca_file             "${LISTEN_TLS_CA_FILE}"
    tls_protocols           "${LISTEN_TLS_PROTOCOLS:-tlsv1.2}"
    client_login_timeout    ${LISTEN_CLIENT_LOGIN_TIMEOUT:-15000}
    target_session_attrs    "${LISTEN_TARGET_SESSION_ATTRS:-any}"
}

storage "${VDB_STORAGE_NAME:-local}" {
    type "local"
}

storage "${STORAGE_NAME:-postgres_server}" {
    type                            "remote"
    host                            "${PG_HOST:-127.0.0.1}"
    port                            ${PG_PORT:-5432}
    tls                             "${STORAGE_TLS_MODE:-disable}"
    tls_cert_file                   "${STORAGE_TLS_CERT_FILE}"
    tls_key_file                    "${STORAGE_TLS_KEY_FILE}"
    tls_ca_file                     "${STORAGE_TLS_CA_FILE}"
    tls_protocols                   "${STORAGE_TLS_PROTOCOLS:-tlsv1.2}"
    endpoints_status_poll_interval  ${ENDPOINTS_STATUS_POLL_INTERVAL:-1000}
    server_max_routing              ${SERVER_MAX_ROUTING:-0}
}

database ${DB_VALUE} {
    user ${USER_VALUE} {
        storage                             "${STORAGE_NAME:-postgres_server}"
        authentication                      "${USER_AUTH_TYPE:-clear_text}"
        password                            "${USER_PASSWORD:-password}"
        pool                                "${POOL_TYPE:-session}"
        pool_size                           ${POOL_SIZE:-128}
        min_pool_size                       ${MIN_POOL_SIZE:-0}
        pool_timeout                        ${POOL_TIMEOUT:-0}
        pool_ttl                            ${POOL_TTL:-0}
        pool_discard                        ${POOL_DISCARD:-no}
        pool_smart_discard                  ${POOL_SMART_DISCARD:-no}
        pool_cancel                         ${POOL_CANCEL:-no}
        pool_rollback                       ${POOL_ROLLBACK:-yes}
        client_fwd_error                    ${CLIENT_FWD_ERROR:-yes}
        application_name_add_host           ${APPLICATION_NAME_ADD_HOST:-no}
        reserve_session_server_connection   ${RESERVE_SESSION_SERVER_CONNECTION:-no}
        server_lifetime                     ${SERVER_LIFETIME:-3600}
        pool_client_idle_timeout            ${POOL_CLIENT_IDLE_TIMEOUT:-0}
        pool_idle_in_transaction_timeout    ${POOL_IDLE_IN_TRANSACTION_TIMEOUT:-0}
        pool_reserve_prepared_statement     ${POOL_RESERVE_PREPARED_STATEMENT:-yes}
        log_debug                           ${USER_LOG_DEBUG:-no}
        maintain_params                     ${MAINTAIN_PARAMS:-no}
        target_session_attrs                "${USER_TARGET_SESSION_ATTRS:-any}"
        ${QUANTILES:+quantiles = "${QUANTILES}"}
    }
}

database ${VIRTUAL_DB_VALUE} {
    user ${VIRTUAL_USER_VALUE} {
        storage         "${VDB_STORAGE_NAME:-local}"
        authentication  "${VIRTUAL_USER_AUTH_TYPE:-none}"
        password        "${VIRTUAL_USER_PASSWORD}"
        pool            "session"
    }
}

${POST_INCLUDE_PATH:+include \"${POST_INCLUDE_PATH}\"}
EOF

    chown odyssey:odyssey "$CONFIG_FILE"
fi

RUN_MODE="${RUN_MODE:-production}"

case "$RUN_MODE" in
    test|debug|root)
        echo "WARNING: Running as root (mode: $RUN_MODE)"
        echo "This should only be used for testing!"
        exec ./odyssey "$CONFIG_FILE"
        ;;

    production|*)
        exec su-exec odyssey ./odyssey "$CONFIG_FILE"
        ;;
esac
