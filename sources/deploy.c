
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
#include <wait_event.h>

static inline int complete_deploy(od_instance_t *instance, od_server_t *server)
{
	uint32_t timeout = instance->config.backend_connect_timeout_ms;

	int rc = od_service_stream_server_until_rfq(server,
						    0 /* ignore errors */,
						    timeout,
						    OD_WAIT_DEPLOY_SERVER_READ);
	if (rc != OD_OK) {
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

/*
 * Switch the backend to the client's client_encoding in a separate Query when
 * the parameters to deploy contain non-ASCII bytes: the backend converts the
 * whole next Query from its current client_encoding before running it.
 */
static int deploy_switch_encoding(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;

	char query[256];
	int query_size = kiwi_vars_cas_encoding(&client->vars, &server->vars,
						query, sizeof(query));
	if (query_size == -1) {
		return -1;
	}
	if (query_size == 0) {
		return 0;
	}
	query_size++; /* terminating NUL */

	machine_msg_t *msg = kiwi_fe_write_query(NULL, query, query_size);
	if (msg == NULL) {
		return -1;
	}
	od_wait_event_start(OD_WAIT_DEPLOY_SERVER_WRITE);
	int rc = od_write(&server->io, msg);
	od_wait_event_end(OD_WAIT_DEPLOY_SERVER_WRITE);
	if (rc == -1) {
		return -1;
	}
	client->server->synced_settings = false;
	od_debug(&instance->logger, client, server, "deploy: %s", query);

	od_server_sync_request(server, 1);
	if (complete_deploy(instance, server) != OK_RESPONSE) {
		return -1;
	}
	return 0;
}

int od_deploy(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;

	if (client->physical_rep || client->logical_rep) {
		return 0;
	}

	if (deploy_switch_encoding(client) == -1) {
		return -1;
	}

	/* compare and set options which are differs from server */
	int query_count;
	query_count = 0;

	char query_buf[1024];
	char *query = NULL;
	int query_size;
	int normalize_search_path;
	query_size = kiwi_vars_cas(&client->vars, &server->vars, &query,
				   query_buf, sizeof(query_buf),
				   instance->config.smart_search_path_enquoting,
				   &normalize_search_path);
	if (query_size == -1) {
		return -1;
	}

	if (query_size > 0) {
		query[query_size] = 0;
		query_size++;
		machine_msg_t *msg;
		msg = kiwi_fe_write_query(NULL, query, query_size);
		if (msg == NULL) {
			if (query != query_buf) {
				od_free(query);
			}
			return -1;
		}

		int rc;
		od_wait_event_start(OD_WAIT_DEPLOY_SERVER_WRITE);
		rc = od_write(&server->io, msg);
		od_wait_event_end(OD_WAIT_DEPLOY_SERVER_WRITE);
		if (rc == -1) {
			if (query != query_buf) {
				od_free(query);
			}
			return -1;
		}

		query_count++;
		client->server->synced_settings = false;

		od_debug(&instance->logger, client, server, "deploy: %s",
			 query);
	} else {
		client->server->synced_settings = true;
	}

	if (query != query_buf) {
		od_free(query);
	}

	if (query_count > 0) {
		od_server_sync_request(server, query_count);

		if (complete_deploy(instance, server) != OK_RESPONSE) {
			return -1;
		}
		if (!normalize_search_path) {
			return OK_RESPONSE;
		}

		/* Cache normalization only when restoration preserves the name list.
		 * Leave unreported parameters (e.g. upstream PG16) untouched. */
		kiwi_var_t *client_path =
			kiwi_vars_get(&client->vars, KIWI_VAR_SEARCH_PATH);
		kiwi_var_t *server_path =
			kiwi_vars_get(&server->vars, KIWI_VAR_SEARCH_PATH);
		if (client_path && server_path &&
		    !kiwi_var_compare(client_path, server_path)) {
			return kiwi_var_set(client_path, KIWI_VAR_SEARCH_PATH,
					    server_path->value,
					    server_path->value_len);
		}
	}

	return OK_RESPONSE;
}
