#ifndef ODYSSEY_H
#define ODYSSEY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include "common_const.h"
#include "sources/c.h"

#include "sources/macro.h"
#ifdef OD_VERSION_BUILD
#	include "sources/build.h"
#endif
#include "sources/atomic.h"
#include "sources/util.h"

#include "sources/debugprintf.h"
#include "sources/grac_shutdown_worker.h"
#include "sources/restart_sync.h"
#include "sources/setproctitle.h"

#include "sources/error.h"
#include "sources/id.h"
#include "sources/list.h"
#include "sources/logger.h"
#include "sources/parser.h"
#include "sources/pid.h"

#include "sources/config.h"
#include "sources/config_common.h"
#include "sources/module.h"
#include "sources/od_dlsym.h"
#include "sources/rules.h"

#include "sources/config_reader.h"
#include "sources/daemon.h"

#include "sources/dns.h"
#include "sources/global.h"
#include "sources/io.h"
#include "sources/msg.h"
#include "sources/postgres.h"
#include "sources/readahead.h"
#include "sources/relay.h"
#include "sources/stat.h"
#include "sources/status.h"
#ifdef USE_SCRAM
#	include "sources/scram.h"
#endif
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/server.h"
#include "sources/server_pool.h"

#include "sources/counter.h"
#include "sources/err_logger.h"

#include "sources/route.h"
#include "sources/route_id.h"
#include "sources/route_pool.h"
#include "sources/router.h"
#include "sources/router_cancel.h"

#include "sources/attribute.h"
#include "sources/auth.h"
#include "sources/auth_query.h"
#include "sources/backend.h"
#include "sources/cancel.h"
#include "sources/compression.h"
#include "sources/console.h"
#include "sources/cron.h"
#include "sources/deploy.h"
#include "sources/frontend.h"
#include "sources/instance.h"
#include "sources/pam.h"
#include "sources/reset.h"
#include "sources/sighandler.h"
#include "sources/system.h"
#include "sources/tdigest.h"
#include "sources/tls.h"
#include "sources/watchdog.h"
#include "sources/worker.h"
#include "sources/worker_pool.h"

#endif /* ODYSSEY_H */
