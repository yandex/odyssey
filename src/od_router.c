
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_daemon.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_msg.h"
#include "od_system.h"
#include "od_instance.h"

#include "od_server.h"
#include "od_server_pool.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od_route_id.h"
#include "od_route.h"

#include "od_pooler.h"
#include "od_relay.h"
#include "od_frontend.h"
#include "od_router.h"

static inline void
od_router(void *arg)
{
	od_router_t *router = arg;
	od_instance_t *instance = router->system->instance;

	od_log(&instance->log, NULL, "router: started");
}

void od_router_init(od_router_t *router, od_system_t *system)
{
	router->machine = -1;
	router->system = system;
}

int od_router_start(od_router_t *router)
{
	od_instance_t *instance = router->system->instance;
	router->machine = machine_create("router", od_router, router);
	if (router->machine == -1) {
		od_error(&instance->log, NULL, "failed to start router");
		return 1;
	}
	return 0;
}
