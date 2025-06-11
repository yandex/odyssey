#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_attach_extended(od_instance_t *instance, char *context,
		       od_router_t *router, od_client_t *client);
