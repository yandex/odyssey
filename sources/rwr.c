
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
	const char *p;
	for (p = lsn; *p != '\0'; ++p) {
		if (*p == '/') {
			slashes++;
			continue;
		}
		if (!isxdigit((unsigned char)*p)) {
			return NOT_OK_RESPONSE;
		}
	}
	if (slashes != 1) {
		return NOT_OK_RESPONSE;
	}
	if (lsn[0] == '\0' || lsn[0] == '/' || p[-1] == '/') {
		return NOT_OK_RESPONSE;
	}
	return OK_RESPONSE;
}

/*
 * Execute a side query. On a backend error, drain the leftover server
 * output up to ReadyForQuery to keep the connection reusable; if the
 * connection is broken, report NOT_OK_RESPONSE.
 */
static od_retcode_t od_rwr_side_query(od_client_t *client, od_server_t *server,
				      char *context, const char *query,
				      machine_msg_t **result)
{
	od_instance_t *instance = client->global->instance;

	machine_msg_t *msg;
	msg = od_query_do(server, context, query, NULL,
			  OD_RWR_QUERY_TIMEOUT_MS);
	if (msg != NULL) {
		*result = msg;
		return OK_RESPONSE;
	}

	if (od_backend_ready_wait(server, context, OD_RWR_QUERY_TIMEOUT_MS) ==
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

	od_storage_endpoint_t *endpoint = server->endpoint;
	if (endpoint == NULL) {
		return OK_RESPONSE;
	}

	od_storage_endpoint_status_t status;
	od_storage_endpoint_status_get(&endpoint->status, &status);

	/* capture LSN only on the primary */
	if (!status.is_read_write) {
		return OK_RESPONSE;
	}

	machine_msg_t *msg = NULL;
	od_retcode_t rc;
	rc = od_rwr_side_query(client, server, "rwr",
			       "SELECT pg_current_wal_insert_lsn()", &msg);
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
	od_storage_endpoint_status_get(&endpoint->status, &status);

	/* reads served by the primary are always consistent */
	if (status.is_read_write) {
		return OK_RESPONSE;
	}

	int server_version_major = od_storage_endpoint_server_version(endpoint);
	if (server_version_major < 19) {
		od_log(&instance->logger, "rwr", client, server,
		       "read-your-writes: WAIT FOR LSN is not supported by "
		       "server version %d",
		       server_version_major);
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
	rc = od_rwr_side_query(client, server, "rwr", query, &msg);
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
