#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */
#include "build.h"

#include "memory.h"

#include "c.h"

#include <regex.h>

#include "types.h"

#ifdef POSTGRESQL_FOUND
#include "postgres.h"
#endif

#include <kiwi/kiwi.h>
#include <machinarium/machinarium.h>

#include "tsa.h"

#include "common_const.h"
#include "misc.h"

#include "attach.h"

#include "macro.h"
#include "atomic.h"
#include "sysv.h"
#include "util.h"

#include "debugprintf.h"
#include "restart_sync.h"
#include "grac_shutdown_worker.h"
#include "setproctitle.h"
#include "sasl.h"

#include "error.h"
#include "list.h"

/* soft oom */
#include "soft_oom.h"

#include "host_watcher.h"

/* hash */
#include "murmurhash.h"
#include "hashmap.h"

#include "pid.h"
#include "id.h"
#include "logger.h"
#include "parser.h"
#include "query_processing.h"

#include "address.h"

#include "global.h"
#include "tls_config.h"
#include "config.h"

#ifdef LDAP_FOUND
#include "ldap_endpoint.h"
#endif

#ifdef PAM_FOUND
#include "pam.h"
#endif

#include "storage.h"
#include "group.h"
#include "pool.h"
#include "rules.h"
#include "hba_rule.h"

#include "config_common.h"

#include "status.h"
#include "readahead.h"
#include "io.h"
#include "dns.h"
#include "attribute.h"

#ifdef POSTGRESQL_FOUND
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include "scram.h"
#endif

#include "relay.h"

#include "tdigest.h"
#include "stat.h"

/* server */
#include "ejection.h"
#include "thread_global.h"
#include "server.h"

/* client */
#include "client.h"

#ifdef LDAP_FOUND
/* LDAP server */
#include "od_ldap.h"
#endif

#include "server_pool.h"
#include "client_pool.h"

#include "multi_pool.h"

/* modules */
#include "module.h"
#include "extension.h"

#include "hba_reader.h"
#include "config_reader.h"

#include "auth.h"
#include "query.h"
#include "auth_query.h"
#include "hba.h"

#include "od_dlsym.h"
#include "daemon.h"

#include "msg.h"

#include "counter.h"
#include "err_logger.h"

#ifdef PROM_FOUND
/* Prometheus metrics */
#include "prom_metrics.h"
#endif
#include "route_id.h"
#include "route.h"
#include "route_pool.h"
#include "router_cancel.h"
#include "router.h"
#include "instance.h"

#include "internal_client.h"

#include "option.h"
#include "cron.h"
#include "system.h"
#include "sighandler.h"

#include "worker.h"
#include "worker_pool.h"

#include "watchdog.h"

/* secure & compression */

#include "tls.h"
#include "compression.h"

#include "cancel.h"
#include "console.h"
#include "reset.h"
#include "deploy.h"

#include "frontend.h"
#include "backend.h"
#include "backend_sync.h"

#include "mdb_iamproxy.h"
#include "external_auth.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define OD_THREAD_LOCAL thread_local
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define OD_THREAD_LOCAL _Thread_local
#elif defined(_MSC_VER)
#define OD_THREAD_LOCAL __declspec(thread)
#else
#define OD_THREAD_LOCAL __thread
#endif
