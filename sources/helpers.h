#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

int od_attach_and_connect_service_client(od_instance_t *instance, char *context,
					 od_router_t *router,
					 od_client_t *client);
