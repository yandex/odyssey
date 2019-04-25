#ifndef ODYSSEY_H
#define ODYSSEY_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <c.h>

// common
#include <memcontext.h>
#include <machinarium.h>
#include <kiwi.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/daemon.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/parser.h"

#include "sources/config.h"
#include "sources/rules.h"
#include "sources/config_reader.h"

#include "sources/msg.h"
#include "sources/global.h"
#include "sources/stat.h"
#include "sources/status.h"
#include "sources/readahead.h"
#include "sources/io.h"
#include "sources/relay.h"
#include "sources/dns.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"

#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/router_cancel.h"
#include "sources/router.h"

#include "sources/instance.h"
#include "sources/cron.h"
#include "sources/system.h"
#include "sources/worker.h"
#include "sources/worker_pool.h"
#include "sources/tls.h"
#include "sources/auth_query.h"
#include "sources/auth.h"
#include "sources/cancel.h"
#include "sources/console.h"
#include "sources/reset.h"
#include "sources/deploy.h"
#include "sources/frontend.h"
#include "sources/backend.h"

#endif /* ODYSSEY_H */
