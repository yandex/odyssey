/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

static inline od_frontend_status_t
od_attach_extended_try_endpoint(od_instance_t *instance, char *context,
				od_router_t *router, od_client_t *client,
				od_storage_endpoint_t *endpoint)
{
	od_router_status_t status;

	status = od_router_attach(router, client, 0 /* wait for idle */,
				  &endpoint->address);
	od_debug(&instance->logger, context, client, NULL,
		 "attaching service client to backend connection status: %s",
		 od_router_status_to_str(status));

	if (status != OD_ROUTER_OK) {
		od_debug(
			&instance->logger, context, client, NULL,
			"failed to attach internal service client to route: %s",
			od_router_status_to_str(status));

		return NOT_OK_RESPONSE;
	}

	od_server_t *server;
	server = client->server;
	od_debug(&instance->logger, context, client, server,
		 "attached to server %s%.*s", server->id.id_prefix,
		 (int)sizeof(server->id.id), server->id.id);

	if (od_backend_not_connected(server)) {
		int rc = od_backend_connect(server, context, NULL, client);
		if (rc == NOT_OK_RESPONSE) {
			od_router_close(router, client);

			return NOT_OK_RESPONSE;
		}
	}

	return OK_RESPONSE;
}

int od_attach_extended(od_instance_t *instance, char *context,
		       od_router_t *router, od_client_t *client)
{
	od_rule_storage_t *storage = client->rule->storage;

	od_endpoint_attach_candidate_t candidates[OD_STORAGE_MAX_ENDPOINTS];
	od_frontend_attach_init_candidates(instance, storage, candidates,
					   OD_TARGET_SESSION_ATTRS_ANY,
					   1 /* prefer localhost */);

	od_frontend_status_t status = OD_EATTACH;

	for (size_t i = 0; i < storage->endpoints_count; ++i) {
		od_storage_endpoint_t *endpoint = candidates[i].endpoint;

		if (candidates[i].priority >= 0) {
			status = od_attach_extended_try_endpoint(
				instance, context, router, client, endpoint);
		}

		if (status == OD_OK) {
			return status;
		}

		char addr[256];
		od_address_to_str(&endpoint->address, addr, sizeof(addr) - 1);

		od_debug(&instance->logger, context, client, NULL,
			 "attach to %s failed with status: %s", addr,
			 od_frontend_status_to_str(status));
	}

	return status;
}
