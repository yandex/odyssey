#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>

od_frontend_status_t
od_attach_extended_endpoint(od_instance_t *instance, char *context,
			    od_router_t *router, od_client_t *client,
			    od_storage_endpoint_t *endpoint);

int od_attach_extended(od_instance_t *instance, char *context,
		       od_router_t *router, od_client_t *client);
