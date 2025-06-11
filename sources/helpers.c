/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

int od_attach_and_connect_service_client(od_instance_t *instance, char *context,
					 od_router_t *router,
					 od_client_t *client)
{
	od_router_status_t status;

	status = od_router_attach(router, client, 0 /* wait for idle */);
	od_debug(&instance->logger, context, client, NULL,
		 "attaching service client to backend connection status: %s",
		 od_router_status_to_str(status));

	if (status != OD_ROUTER_OK) {
		od_debug(
			&instance->logger, context, client, NULL,
			"failed to attach internal auth query client to route: %s",
			od_router_status_to_str(status));

		return NOT_OK_RESPONSE;
	}

	od_server_t *server;
	server = client->server;
	od_debug(&instance->logger, context, client, server,
		 "attached to server %s%.*s", server->id.id_prefix,
		 (int)sizeof(server->id.id), server->id.id);

	if (od_backend_not_connected(server)) {
		int rc = od_backend_connect_service(server, context, NULL,
						    client);
		if (rc == NOT_OK_RESPONSE) {
			od_router_close(router, client);

			return NOT_OK_RESPONSE;
		}
	}

	return OK_RESPONSE;
}
