
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <types.h>
#include <client.h>
#include <global.h>
#include <server.h>
#include <route.h>
#include <instance.h>
#include <stream.h>

static int complete_deploy(od_server_t *server, uint32_t timeout_ms)
{
	od_frontend_status_t status =
		od_service_stream_server_until_rfq("deploy", server, timeout_ms);
	if (status != OD_OK) {
		od_gerror("deploy", server->client, server,
			  "server deploy error: %d (%s), errno=%d (%s)", status,
			  od_frontend_status_to_str(status), machine_errno(),
			  strerror(machine_errno()));
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

int od_deploy(od_client_t *client, char *context)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;
	od_route_t *route = client->route;

	if (route->id.physical_rep || route->id.logical_rep) {
		return 0;
	}

	/* compare and set options which are differs from server */
	int query_count;
	query_count = 0;

	char query[OD_QRY_MAX_SZ];
	int query_size;
	query_size =
		kiwi_vars_cas(&client->vars, &server->vars, query,
			      sizeof(query) - 1,
			      instance->config.smart_search_path_enquoting);

	if (query_size > 0) {
		query[query_size] = 0;
		query_size++;
		machine_msg_t *msg;
		msg = kiwi_fe_write_query(NULL, query, query_size);
		if (msg == NULL) {
			return -1;
		}

		int rc;
		rc = od_write(&server->io, msg);
		if (rc == -1) {
			return -1;
		}

		query_count++;
		client->server->synced_settings = false;

		od_debug(&instance->logger, context, client, server,
			 "deploy: %s", query);
	} else {
		client->server->synced_settings = true;
	}

	if (query_count > 0) {
		od_server_sync_request(server, query_count);

		/* TODO: parametrize timeout from config */
		return complete_deploy(server, 1000);
	}

	return OK_RESPONSE;
}
