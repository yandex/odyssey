
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>
#include <machinarium/machinarium.h>

#include <rwr.h>
#include <global.h>
#include <instance.h>
#include <server.h>
#include <backend.h>
#include <storage.h>
#include <query.h>
#include <util.h>

static od_retcode_t od_rwr_parse_text_datarow(machine_msg_t *msg, char *buf,
					      size_t buf_size)
{
	char *pos = (char *)machine_msg_data(msg) + 1;
	uint32_t pos_size = machine_msg_size(msg) - 1;

	/* size */
	uint32_t size;
	int rc;
	rc = kiwi_read32(&size, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	/* count */
	uint16_t count;
	rc = kiwi_read16(&count, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	/* we expect exactly one column */
	if (count != 1) {
		goto error;
	}

	uint32_t len;
	rc = kiwi_read32(&len, &pos, &pos_size);
	if (kiwi_unlikely(rc == -1)) {
		goto error;
	}

	/* SQL NULL is encoded as length = -1 (0xFFFFFFFF in uint32_t) */
	if (len == (uint32_t)-1) {
		goto error;
	}

	/* the declared column length must fit the rest of the message */
	if (len > pos_size) {
		goto error;
	}

	if (len + 1 > buf_size) {
		goto error;
	}

	memcpy(buf, pos, len);
	buf[len] = '\0';

	return OK_RESPONSE;
error:
	return NOT_OK_RESPONSE;
}

static od_retcode_t od_rwr_lsn_is_valid(const char *lsn)
{
	/* pg_lsn format: [0-9A-F]{1,8}/[0-9A-F]{1,8}, e.g. 0/302CD20 */
	int slashes = 0;
	int digits = 0;
	bool nonzero = false;
	const char *p;
	for (p = lsn; *p != '\0'; ++p) {
		if (*p == '/') {
			if (++slashes != 1 || digits == 0) {
				return NOT_OK_RESPONSE;
			}
			digits = 0;
			continue;
		}
		if (!isxdigit((unsigned char)*p) || ++digits > 8) {
			return NOT_OK_RESPONSE;
		}
		nonzero |= *p != '0';
	}
	if (slashes != 1 || digits == 0 || !nonzero) {
		return NOT_OK_RESPONSE;
	}
	return OK_RESPONSE;
}

void od_rwr_receive_lsn(od_server_t *server, const char *value, size_t len)
{
	od_client_t *client = server->client;
	server->rwr_lsn_feedback = false;
	if (client == NULL || !client->rwr_consistent || len < 2 ||
	    len > sizeof(client->rwr_last_lsn) || value[len - 1] != '\0' ||
	    strlen(value) != len - 1 ||
	    od_rwr_lsn_is_valid(value) != OK_RESPONSE) {
		return;
	}

	memcpy(client->rwr_last_lsn, value, len);
	client->rwr_last_lsn_valid = true;
	server->rwr_lsn_feedback = true;
}

/*
 * Upper bound for the rwr side queries (ms): the per-storage
 * rwr_wait_timeout option; 0 means no limit
 */
static uint32_t od_rwr_timeout_ms(od_storage_endpoint_t *endpoint)
{
	od_rule_storage_t *storage = endpoint->storage;
	if (storage == NULL || storage->rwr_wait_timeout_ms <= 0) {
		return UINT32_MAX;
	}
	return (uint32_t)storage->rwr_wait_timeout_ms;
}

/*
 * Read the endpoint status, live-refreshing it as od_backend_check_tsa
 * does (the cached status may still hold the constructor defaults, e.g.
 * a replica is misreported as a primary until the first poll)
 */
static od_retcode_t od_rwr_endpoint_status(od_client_t *client,
					   od_server_t *server,
					   od_storage_endpoint_t *endpoint,
					   od_storage_endpoint_status_t *status)
{
	od_rule_storage_t *storage = endpoint->storage;
	uint64_t poll_interval_ms = 0ULL;
	if (storage != NULL && storage->endpoints_status_poll_interval_ms > 0) {
		poll_interval_ms =
			(uint64_t)storage->endpoints_status_poll_interval_ms;
	}
	if (od_storage_endpoint_status_is_outdated(&endpoint->status,
						   poll_interval_ms)) {
		od_instance_t *instance = client->global->instance;
		if (od_backend_update_endpoint_status(instance, client, server,
						      "rwr", NULL, endpoint) !=
		    OK_RESPONSE) {
			/*
			 * A failed refresh must not kill the client
			 * session: drain the leftover server output (the
			 * failed refresh may have left some) and proceed
			 * with the stale cached status
			 */
			if (machine_timedout()) {
				od_error(&instance->logger, "rwr", client,
					 server,
					 "failed to refresh storage endpoint "
					 "status: timed out, dropping backend "
					 "connection");
				return NOT_OK_RESPONSE;
			}
			if (od_backend_ready_wait(server, "rwr",
						  od_rwr_timeout_ms(endpoint)) ==
			    NOT_OK_RESPONSE) {
				od_error(&instance->logger, "rwr", client,
					 server,
					 "failed to refresh storage endpoint "
					 "status: connection is broken");
				return NOT_OK_RESPONSE;
			}
			od_error(&instance->logger, "rwr", client, server,
				 "failed to refresh storage endpoint "
				 "status, proceeding with the cached one");
		}
	}

	od_storage_endpoint_status_get(&endpoint->status, status);
	return OK_RESPONSE;
}

/*
 * Execute a side query. On a backend error, drain the leftover server
 * output up to ReadyForQuery to keep the connection reusable; if the
 * query timed out the connection cannot be reused (the result will
 * arrive later and corrupt the stream) - report NOT_OK_RESPONSE.
 */
static od_retcode_t od_rwr_side_query(od_client_t *client, od_server_t *server,
				      char *context, const char *query,
				      uint32_t timeout_ms,
				      machine_msg_t **result)
{
	od_instance_t *instance = client->global->instance;

	machine_msg_t *msg;
	msg = od_query_do(server, context, query, NULL, timeout_ms);
	if (msg != NULL) {
		*result = msg;
		return OK_RESPONSE;
	}

	if (machine_timedout()) {
		od_error(&instance->logger, context, client, server,
			 "failed to execute '%s': timed out, dropping "
			 "backend connection",
			 query);
		return NOT_OK_RESPONSE;
	}

	if (od_backend_ready_wait(server, context, timeout_ms) ==
	    NOT_OK_RESPONSE) {
		od_error(&instance->logger, context, client, server,
			 "failed to execute '%s': connection is broken", query);
		return NOT_OK_RESPONSE;
	}

	od_error(&instance->logger, context, client, server,
		 "failed to execute '%s': connection is still usable", query);
	*result = NULL;
	return OK_RESPONSE;
}

od_retcode_t od_rwr_capture_lsn(od_client_t *client, od_server_t *server)
{
	if (!client->rwr_consistent) {
		return OK_RESPONSE;
	}

	od_instance_t *instance = client->global->instance;

	/*
	 * Side queries are not allowed inside a client transaction: they
	 * would break the client's snapshot.
	 */
	if (server->is_transaction) {
		return OK_RESPONSE;
	}
	if (server->rwr_lsn_feedback) {
		return OK_RESPONSE;
	}

	od_storage_endpoint_t *endpoint = server->endpoint;
	if (endpoint == NULL) {
		return OK_RESPONSE;
	}

	od_storage_endpoint_status_t status;
	if (od_rwr_endpoint_status(client, server, endpoint, &status) !=
	    OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	/* capture LSN only on the primary */
	if (!status.is_read_write || !status.alive) {
		return OK_RESPONSE;
	}

	machine_msg_t *msg = NULL;
	od_retcode_t rc;
	rc = od_rwr_side_query(client, server, "rwr",
			       "SELECT pg_current_wal_insert_lsn()",
			       od_rwr_timeout_ms(endpoint), &msg);
	if (rc != OK_RESPONSE) {
		client->rwr_last_lsn_valid = false;
		return NOT_OK_RESPONSE;
	}
	if (msg == NULL) {
		client->rwr_last_lsn_valid = false;
		return OK_RESPONSE;
	}

	char lsn[OD_RWR_LSN_MAX];
	rc = od_rwr_parse_text_datarow(msg, lsn, sizeof(lsn));
	machine_msg_free(msg);
	if (rc != OK_RESPONSE || od_rwr_lsn_is_valid(lsn) != OK_RESPONSE) {
		od_error(&instance->logger, "rwr", client, server,
			 "can't parse pg_current_wal_insert_lsn result");
		client->rwr_last_lsn_valid = false;
		return OK_RESPONSE;
	}

	memcpy(client->rwr_last_lsn, lsn, sizeof(lsn));
	client->rwr_last_lsn_valid = true;

	od_debug(&instance->logger, "rwr", client, server,
		 "read-your-writes: captured lsn %s", lsn);

	return OK_RESPONSE;
}

od_retcode_t od_rwr_wait_lsn(od_client_t *client, od_server_t *server)
{
	if (!client->rwr_consistent || !client->rwr_last_lsn_valid) {
		return OK_RESPONSE;
	}

	od_instance_t *instance = client->global->instance;

	/* WAIT FOR LSN must not be sent inside a transaction */
	if (server->is_transaction) {
		return OK_RESPONSE;
	}

	od_storage_endpoint_t *endpoint = server->endpoint;
	if (endpoint == NULL) {
		return OK_RESPONSE;
	}

	od_storage_endpoint_status_t status;
	if (od_rwr_endpoint_status(client, server, endpoint, &status) !=
	    OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	/* reads served by the primary are always consistent */
	if (status.is_read_write || !status.alive) {
		return OK_RESPONSE;
	}

	int server_version_major = od_storage_endpoint_server_version(endpoint);
	if (server_version_major < 19) {
		if (!endpoint->rwr_wait_unsupported_logged) {
			endpoint->rwr_wait_unsupported_logged = 1;
			od_log(&instance->logger, "rwr", client, server,
			       "read-your-writes: WAIT FOR LSN is not supported "
			       "by server version %d",
			       server_version_major);
		} else {
			od_debug(&instance->logger, "rwr", client, server,
				 "read-your-writes: WAIT FOR LSN is not "
				 "supported by server version %d",
				 server_version_major);
		}
		return OK_RESPONSE;
	}

	if (od_rwr_lsn_is_valid(client->rwr_last_lsn) != OK_RESPONSE) {
		return OK_RESPONSE;
	}

	char query[OD_RWR_LSN_MAX + sizeof("WAIT FOR LSN ''")];
	od_snprintf(query, sizeof(query), "WAIT FOR LSN '%s'",
		    client->rwr_last_lsn);

	machine_msg_t *msg = NULL;
	od_retcode_t rc;
	rc = od_rwr_side_query(client, server, "rwr", query,
			       od_rwr_timeout_ms(endpoint), &msg);
	if (msg != NULL) {
		machine_msg_free(msg);
	}
	if (rc != OK_RESPONSE) {
		return NOT_OK_RESPONSE;
	}

	/*
	 * msg == NULL means the WAIT failed with a backend error, e.g. the
	 * endpoint was promoted to a primary; proceed without waiting.
	 */
	return OK_RESPONSE;
}
