
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
#include <storage.h>
#include <rules.h>
#include <util.h>
#include <xplan.h>

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
 * Rule overrides the global timeout, including an explicit 0 (no limit).
 */
uint32_t od_rwr_timeout_ms(od_client_t *client)
{
	od_instance_t *instance = client->global->instance;
	uint64_t timeout_ms = instance->config.rwr_wait_timeout_ms;
	if (client->rule->rwr_wait_timeout_ms >= 0) {
		timeout_ms = client->rule->rwr_wait_timeout_ms;
	}
	return timeout_ms == 0 ? UINT32_MAX : (uint32_t)timeout_ms;
}

void od_rwr_capture_lsn_result(od_client_t *client, machine_msg_t *msg)
{
	od_instance_t *instance = client->global->instance;
	char lsn[OD_RWR_LSN_MAX];
	if (od_rwr_parse_text_datarow(msg, lsn, sizeof(lsn)) != OK_RESPONSE ||
	    od_rwr_lsn_is_valid(lsn) != OK_RESPONSE) {
		od_error(&instance->logger, client, client->server,
			 "can't parse pg_current_wal_insert_lsn result");
		client->rwr_last_lsn_valid = false;
		return;
	}

	od_snprintf(client->rwr_last_lsn, sizeof(client->rwr_last_lsn), "%s",
		    lsn);
	client->rwr_last_lsn_valid = true;
	od_debug(&instance->logger, client, client->server,
		 "read-your-writes: captured lsn %s", lsn);
}

od_retcode_t od_rwr_capture_lsn(od_client_t *client, od_server_t *server)
{
	if (client == NULL || !client->rwr_consistent ||
	    server->is_transaction || server->endpoint == NULL ||
	    server->in_hot_standby == 1) {
		return OK_RESPONSE;
	}

	if (od_xplan_capture_lsn(&client->relay, od_rwr_timeout_ms(client)) !=
	    OD_OK) {
		client->rwr_last_lsn_valid = false;
		return NOT_OK_RESPONSE;
	}
	return OK_RESPONSE;
}

od_frontend_status_t od_rwr_prepare_wait(od_client_t *client,
					 machine_msg_t **msg)
{
	*msg = NULL;
	od_server_t *server = client->server;
	if (!server->rwr_wait_pending) {
		return OD_OK;
	}
	server->rwr_wait_pending = false;

	if (!client->rwr_consistent || !client->rwr_last_lsn_valid ||
	    server->is_transaction || server->in_hot_standby != 1 ||
	    server->endpoint == NULL) {
		return OD_OK;
	}

	od_instance_t *instance = client->global->instance;
	od_storage_endpoint_t *endpoint = server->endpoint;
	int version = od_storage_endpoint_server_version(endpoint);
	if (version < 19) {
		if (!atomic_exchange(&endpoint->rwr_wait_unsupported_logged,
				     true)) {
			od_log(&instance->logger, client, server,
			       "read-your-writes: WAIT FOR LSN is not supported "
			       "by server version %d",
			       version);
		}
		return OD_OK;
	}

	if (od_rwr_lsn_is_valid(client->rwr_last_lsn) != OK_RESPONSE) {
		return OD_OK;
	}

	char query[OD_RWR_LSN_MAX + sizeof("WAIT FOR LSN ''")];
	od_snprintf(query, sizeof(query), "WAIT FOR LSN '%s'",
		    client->rwr_last_lsn);
	*msg = kiwi_fe_write_query(NULL, query, strlen(query) + 1);
	return *msg != NULL ? OD_OK : OD_EOOM;
}
