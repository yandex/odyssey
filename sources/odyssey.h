#ifndef ODYSSEY_H
#define ODYSSEY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>

#include "sources/c.h"

#include "sources/common_const.h"
#include "sources/misc.h"

#include "sources/macro.h"
#include "sources/build.h"
#include "sources/atomic.h"
#include "sources/sysv.h"
#include "sources/util.h"

#include "sources/debugprintf.h"
#include "sources/restart_sync.h"
#include "sources/grac_shutdown_worker.h"
#include "sources/setproctitle.h"

#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/parser.h"

#include "sources/global.h"
#include "sources/config.h"

#ifdef LDAP_FOUND
#include "sources/ldap_endpoint.h"
#endif

#include "sources/pam.h"
#include "sources/rules.h"

#include "sources/config_common.h"

#include "sources/status.h"
#include "sources/readahead.h"
#include "sources/io.h"
#include "sources/dns.h"
#include "sources/postgres.h"

#include "sources/attribute.h"

#ifdef USE_SCRAM
#include "sources/scram.h"
#endif

#include "sources/relay.h"

#include "sources/tdigest.h"
#include "sources/stat.h"

/* server */
#include "sources/ejection.h"
#include "sources/thread_global.h"
#include "sources/server.h"

/* client */
#include "sources/client.h"

#ifdef LDAP_FOUND
/* LDAP server */
#include "sources/od_ldap.h"
#endif

#include "sources/server_pool.h"
#include "sources/client_pool.h"

/* modules */
#include "sources/module.h"
#include "sources/extention.h"

#include "sources/config_reader.h"

#include "sources/auth.h"
#include "sources/auth_query.h"

#include "sources/od_dlsym.h"
#include "sources/daemon.h"

#include "sources/msg.h"

#include "sources/counter.h"
#include "sources/err_logger.h"

#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/router_cancel.h"
#include "sources/router.h"
#include "sources/instance.h"
#include "sources/option.h"
#include "sources/cron.h"
#include "sources/system.h"
#include "sources/sighandler.h"

#include "sources/watchdog.h"

#include "sources/worker.h"
#include "sources/worker_pool.h"

/* secure & compression */

#include "sources/tls.h"
#include "sources/compression.h"

#include "sources/cancel.h"
#include "sources/console.h"
#include "sources/reset.h"
#include "sources/deploy.h"

#include "sources/frontend.h"
#include "sources/backend.h"

#endif /* ODYSSEY_H */
