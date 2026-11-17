
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
#include <backend.h>
#include <deploy.h>

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

/* BEGIN was acknowledged virtually; suppress its success response. */
static int deploy_begin(od_client_t *client, uint32_t timeout)
{
	od_server_t *server = client->server;
	machine_msg_t *msg = kiwi_fe_write_query(
		NULL, client->pending_begin, strlen(client->pending_begin) + 1);
	if (msg == NULL) {
		return -1;
	}
	if (od_write(&server->io, msg) != 0) {
		return -1;
	}
	od_server_sync_request(server, 1);

	int failed = 0;
	for (;;) {
		msg = od_read(&server->io, timeout, OD_READ_BE);
		if (msg == NULL) {
			return -1;
		}
		char *data = machine_msg_data(msg);
		int size = machine_msg_size(msg);
		kiwi_be_type_t type = *data;

		if (type == KIWI_BE_READY_FOR_QUERY) {
			int rc = od_backend_ready(server, data, size);
			od_client_pending_begin_clear(client);
			if (rc != 0) {
				machine_msg_free(msg);
				return -1;
			}
			if (!failed) {
				machine_msg_free(msg);
				return OK_RESPONSE;
			}
			/* The error and RFQ are the response to the current query. */
			return od_write2(&client->io, msg, timeout) == 0 ?
				       OD_DEPLOY_BEGIN_ERROR : -1;
		}
		if (type == KIWI_BE_COMMAND_COMPLETE) {
			machine_msg_free(msg);
			continue;
		}
		if (type == KIWI_BE_ERROR_RESPONSE) {
			od_backend_error(server, data, size);
			failed = 1;
		} else if (type == KIWI_BE_PARAMETER_STATUS &&
			   od_backend_update_parameter(server, data, size, 0) != 0) {
			machine_msg_free(msg);
			return -1;
		}
		if (od_write2(&client->io, msg, timeout) != 0) {
			return -1;
		}
	}
}

int od_deploy(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;

	if (client->physical_rep || client->logical_rep) {
		return 0;
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
		/* Cache normalization only when restoration preserves the name list.
		 * Leave unreported parameters (e.g. upstream PG16) untouched. */
		kiwi_var_t *client_path =
			kiwi_vars_get(&client->vars, KIWI_VAR_SEARCH_PATH);
		kiwi_var_t *server_path =
			kiwi_vars_get(&server->vars, KIWI_VAR_SEARCH_PATH);
		if (normalize_search_path && client_path && server_path &&
		    !kiwi_var_compare(client_path, server_path)) {
			if (kiwi_var_set(client_path, KIWI_VAR_SEARCH_PATH,
					 server_path->value,
					 server_path->value_len) != 0) {
				return -1;
			}
		}
	}

	/* Restore session parameters before entering the transaction. */
	if (client->pending_begin) {
		return deploy_begin(client,
				    instance->config.backend_connect_timeout_ms);
	}

	return OK_RESPONSE;
}
