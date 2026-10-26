/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>
#include <machinarium/msg.h>

#include <rules.h>
#include <route.h>
#include <dns.h>
#include <backend.h>
#include <od_memory.h>
#include <relay.h>
#include <io.h>
#include <util.h>
#include <frontend.h>
#include <instance.h>
#include <global.h>
#include <stream.h>
#include <xplan.h>
#include <pstmt.h>
#include <misc.h>
#include <option.h>
#include <worker.h>
#include <query.h>
#include <tuple_table_slot.h>
#include <console.h>
#include <virtual.h>

#define APPLICATION_NAME_STR "application_name"
#define ODYSSEY_TARGET_SESSION_ATTRS_STR "odyssey.target_session_attrs"
#define ODYSSEY_EXECUTE_ON_HOST_STR "odyssey.execute_on_host"
#define ODYSSEY_PIN_BACKEND "odyssey.pin_backend"
#define ODYSSEY_RWR_CONSISTENT "odyssey.rwr_consistent"
#define ODYSSEY_OPPORTUNISTIC_ACQUIRE "odyssey.opportunistic_acquire"
#define ODYSSEY_POOLING_MODE "odyssey.pooling_mode"
#define ODYSSEY_VERSION_STR "odyssey.version"
#define PROCESSED_BY_ODYSSEY_STR "processed virtually by odyssey"
/* the formatted virtual ErrorResponse message buffer */
#define VIRTUAL_ERROR_BUF_SIZE 128

/*
 * Virtual portals.
 *
 * The query is answered by odyssey itself, without the backend. The
 * builders below never touch the client io: they only compose the
 * answer into an od_virtual_portal_t (the tuples, when any, go into
 * its od_tuple_table_slot_t). The single write to the client happens
 * in try_virtual_process_query().
 *
 * The builders keep the regular od_frontend_status_t contracts:
 *
 *   OD_OK       - not virtual, forward the query to the backend
 *   OD_SKIP     - the portal is composed in portal, has to be written
 *   OD_ATTACH   - attach first, then retry
 *   OD_REPLACED - the query was replaced with another one (appname),
 *                 the portal must not be written
 */
void virtual_portal_init(od_virtual_portal_t *portal)
{
	memset(portal, 0, sizeof(*portal));
}

void virtual_portal_free(od_virtual_portal_t *portal)
{
	if (portal->tts != NULL) {
		od_tuple_table_slot_free(portal->tts);
		portal->tts = NULL;
	}
	if (portal->notice != NULL) {
		machine_msg_free(portal->notice);
		portal->notice = NULL;
	}
	if (portal->command_complete != NULL) {
		machine_msg_free(portal->command_complete);
		portal->command_complete = NULL;
	}
	if (portal->error != NULL) {
		machine_msg_free(portal->error);
		portal->error = NULL;
	}
}

/*
 * the NOTICE the virtual GUC processing prepends to its answers
 */
static int virtual_portal_set_notice(od_virtual_portal_t *portal)
{
	if (portal->notice != NULL) {
		return 0;
	}

	machine_msg_t *msg =
		kiwi_be_write_notice(NULL, 'M', PROCESSED_BY_ODYSSEY_STR);
	if (msg == NULL) {
		return -1;
	}

	portal->notice = msg;
	return 0;
}

/* the CommandComplete for the virtually executed command */
static int virtual_portal_set_complete(od_virtual_portal_t *portal,
				       const char *tag)
{
	machine_msg_t *msg = kiwi_be_write_command_complete(NULL, tag);
	if (msg == NULL) {
		return -1;
	}

	portal->command_complete = msg;
	return 0;
}

static int virtual_portal_is_error(const od_virtual_portal_t *portal)
{
	return portal->error != NULL;
}

__attribute__((format(printf, 3, 4))) static int
virtual_portal_set_errorf(od_virtual_portal_t *portal, const char *sqlstate,
			  const char *fmt, ...)
{
	/* a virtually processed answer always leads with the NOTICE */
	if (virtual_portal_set_notice(portal) != 0) {
		return -1;
	}

	char buf[VIRTUAL_ERROR_BUF_SIZE];
	va_list args;
	va_start(args, fmt);
	int len = od_vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (len < 0) {
		return -1;
	}

	machine_msg_t *msg = kiwi_be_write_error_as(
		NULL, "ERROR", sqlstate, NULL, 0, NULL, 0, buf, (size_t)len, 0);
	if (msg == NULL) {
		return -1;
	}

	portal->error = msg;
	return 0;
}

/* reject an invalid value of a virtual GUC */
static int virtual_portal_reject_guc(od_virtual_portal_t *portal,
				     const char *guc_name,
				     const char *option_value,
				     size_t option_value_len)
{
	return virtual_portal_set_errorf(
		portal, KIWI_INVALID_PARAMETER_VALUE,
		"invalid value for parameter \"%s\": \"%.*s\"", guc_name,
		(int)option_value_len, option_value);
}

/*
 * a single-column, single-row text result of a virtual SHOW, stored as
 * a tuple in the portal slot
 */
static int virtual_portal_set_str_column(od_virtual_portal_t *portal,
					 const char *name, const char *value)
{
	if (portal->tts == NULL) {
		portal->tts = od_tuple_table_slot_create();
		if (portal->tts == NULL) {
			return -1;
		}
	}

	machine_msg_t *msg = kiwi_be_write_row_descriptionf(
		portal->tts->row_description, "s", name);
	if (msg == NULL) {
		return -1;
	}

	int offset;
	msg = kiwi_be_write_data_row(portal->tts->data_rows, &offset);
	if (msg == NULL) {
		return -1;
	}

	int rc;
	rc = kiwi_be_write_data_row_add(portal->tts->data_rows, offset, value,
					strlen(value));
	if (rc != 0) {
		return -1;
	}

	return 0;
}

static od_frontend_status_t process_set_appname(od_client_t *client,
						const od_query_ctx_t *ctx)
{
	int rc;
	char original_appname[64];
	size_t len = od_min(strlen(ctx->s2), sizeof(original_appname));
	snprintf(original_appname, sizeof(original_appname), "%s", ctx->s2);

	char peer_name[KIWI_MAX_VAR_SIZE];
	rc = od_getpeername(client->io.io, peer_name, sizeof(peer_name), 1, 0);
	if (rc != 0) {
		od_gerror("query", client, client->server,
			  "can't get peer name, errno = %d (%s)",
			  mm_errno_get(), strerror(mm_errno_get()));
		goto error;
	}

	if (client->server == NULL) {
		/* we will write to server - need to attach if not yet */
		return OD_ATTACH;
	}

	char suffix[KIWI_MAX_VAR_SIZE];
	od_snprintf(suffix, sizeof(suffix), " - %s", peer_name);

	char appname[64];
	int appname_len = od_concat_prefer_right(appname, sizeof(appname),
						 original_appname, len, suffix,
						 strlen(suffix));

	char query[128];
	od_snprintf(query, sizeof(query), "set application_name to \'%.*s\';",
		    appname_len, appname);

	machine_msg_t *msg;
	msg = kiwi_fe_write_query(NULL, query, strlen(query) + 1);
	if (msg == NULL) {
		od_gerror("query", client, client->server,
			  "can't create message to send \"%s\"", query);
		return OD_EOOM;
	}

	rc = od_write(&client->server->io, msg);
	if (rc != 0) {
		od_gerror("query", client, client->server,
			  "can't write \"%s\", rc = %d, errno = %d", query, rc,
			  machine_errno());
		return OD_ESERVER_WRITE;
	}

	od_server_sync_request(client->server, 1);

	return OD_REPLACED;

error:
	/* can't handle, let pg do plain version of the query */
	return OD_OK;
}

static od_frontend_status_t build_set_generic_bool(od_client_t *client,
						   const od_query_ctx_t *ctx,
						   od_virtual_portal_t *portal,
						   bool *guc_val_ptr,
						   bool *origin_to_reset)
{
	const char *option_value;
	size_t option_value_len;

	od_instance_t *instance = client->global->instance;

	option_value = ctx->s2;
	option_value_len = strlen(option_value);

	bool value;
	if (!parse_bool_with_len(option_value, option_value_len, &value)) {
		/* Reject this */
		if (virtual_portal_reject_guc(portal, ctx->s1, option_value,
					      option_value_len) != 0) {
			return OD_EOOM;
		}
		return OD_SKIP;
	}

	*guc_val_ptr = value;
	if (origin_to_reset != NULL) {
		*origin_to_reset = 0;
	}

	od_debug(&instance->logger, "virtual processing", client,
		 client->server, "processed virtual bool GUC %.*s",
		 (int)option_value_len, option_value);

	if (virtual_portal_set_notice(portal) != 0) {
		return OD_EOOM;
	}
	if (virtual_portal_set_complete(portal, "SET") != 0) {
		return OD_EOOM;
	}
	return OD_SKIP;
}

static od_frontend_status_t build_set_tsa(od_client_t *client,
					  const od_query_ctx_t *ctx,
					  od_virtual_portal_t *portal)
{
	const char *option_value;
	size_t option_value_len;
	od_server_t *server;
	od_instance_t *instance = client->global->instance;

	server = client->server;

	option_value = ctx->s2;
	option_value_len = strlen(option_value);

	/* for now, very straightforward logic, as there is only one supported param */
	if (option_value_len == strlen("read-only") &&
	    strncasecmp(option_value, "read-only", option_value_len) == 0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      "read-only", strlen("read-only"));
	} else if (option_value_len == strlen("read-write") &&
		   strncasecmp(option_value, "read-write", option_value_len) ==
			   0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      "read-write", strlen("read-write"));
	} else if (option_value_len == strlen("any") &&
		   strncasecmp(option_value, "any", option_value_len) == 0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS, "any",
			      strlen("any"));
	} else if (option_value_len == strlen("prefer-standby") &&
		   strncasecmp(option_value, "prefer-standby",
			       option_value_len) == 0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      "prefer-standby", strlen("prefer-standby"));
	} else {
		od_debug(&instance->logger, "virtual processing", client,
			 server, "unsupported tsa hint %.*s",
			 (int)option_value_len, option_value);

		if (virtual_portal_reject_guc(
			    portal, ODYSSEY_TARGET_SESSION_ATTRS_STR,
			    option_value, option_value_len) != 0) {
			return OD_EOOM;
		}
		return OD_SKIP;
	}

	od_debug(&instance->logger, "virtual processing", client, server,
		 "parsed tsa hint %.*s", (int)option_value_len, option_value);

	if (virtual_portal_set_notice(portal) != 0) {
		return OD_EOOM;
	}
	if (virtual_portal_set_complete(portal, "SET") != 0) {
		return OD_EOOM;
	}
	return OD_SKIP;
}

/*
 * SET odyssey.execute_on_host TO 'host[:port]' - attach only to this storage
 * host, bypassing balancing; empty value (or DEFAULT) resets the hint
 */
static od_frontend_status_t
build_set_execute_on_host(od_client_t *client, const od_query_ctx_t *ctx,
			  od_virtual_portal_t *portal)
{
	od_instance_t *instance = client->global->instance;
	const char *value = ctx->s2;
	size_t value_len = strlen(value);

	if (value_len == 0) {
		od_address_destroy(&client->execute_on_host);
		od_address_init(&client->execute_on_host);

		od_debug(&instance->logger, "virtual processing", client,
			 client->server, "execute_on_host hint is reset");

		if (virtual_portal_set_notice(portal) != 0) {
			return OD_EOOM;
		}
		if (virtual_portal_set_complete(portal, "SET") != 0) {
			return OD_EOOM;
		}
		return OD_SKIP;
	}

	od_address_t *addrs = NULL;
	size_t count = 0;
	if (od_parse_addresses(value, &addrs, &count) != OK_RESPONSE) {
		if (virtual_portal_reject_guc(portal,
					      ODYSSEY_EXECUTE_ON_HOST_STR,
					      value, value_len) != 0) {
			return OD_EOOM;
		}
		return OD_SKIP;
	}

	if (count != 1 || !od_frontend_host_is_known(client, &addrs[0])) {
		for (size_t i = 0; i < count; ++i) {
			od_address_destroy(&addrs[i]);
		}
		od_free(addrs);

		od_debug(&instance->logger, "virtual processing", client,
			 client->server, "unknown execute_on_host %.*s",
			 (int)value_len, value);

		if (virtual_portal_reject_guc(portal,
					      ODYSSEY_EXECUTE_ON_HOST_STR,
					      value, value_len) != 0) {
			return OD_EOOM;
		}
		return OD_SKIP;
	}

	/* od_address_move destroys the previous value */
	od_address_move(&client->execute_on_host, &addrs[0]);
	od_free(addrs);

	od_debug(&instance->logger, "virtual processing", client,
		 client->server, "parsed execute_on_host hint %.*s",
		 (int)value_len, value);

	if (virtual_portal_set_notice(portal) != 0) {
		return OD_EOOM;
	}
	if (virtual_portal_set_complete(portal, "SET") != 0) {
		return OD_EOOM;
	}
	return OD_SKIP;
}

static od_frontend_status_t process_vset(od_client_t *client,
					 const od_query_ctx_t *ctx,
					 od_virtual_portal_t *portal)
{
	od_instance_t *instance = client->global->instance;

	if (strcmp(ctx->s1, APPLICATION_NAME_STR) == 0) {
		if (client->rule->application_name_add_host) {
			return process_set_appname(client, ctx);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_TARGET_SESSION_ATTRS_STR) == 0) {
		if (instance->config.virtual_processing) {
			return build_set_tsa(client, ctx, portal);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_EXECUTE_ON_HOST_STR) == 0) {
		if (instance->config.virtual_processing) {
			return build_set_execute_on_host(client, ctx, portal);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_PIN_BACKEND) == 0) {
		if (instance->config.virtual_processing) {
			return build_set_generic_bool(
				client, ctx, portal, &client->backend_pin,
				&client->backend_pin_listen);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_RWR_CONSISTENT) == 0) {
		if (instance->config.virtual_processing) {
			od_frontend_status_t status =
				build_set_generic_bool(client, ctx, portal,
						       &client->rwr_consistent,
						       NULL);
			/* Reasserting "on" must preserve the last write position. */
			if (!client->rwr_consistent) {
				client->rwr_last_lsn_valid = false;
			}
			return status;
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_OPPORTUNISTIC_ACQUIRE) == 0) {
		if (instance->config.virtual_processing) {
			return build_set_generic_bool(
				client, ctx, portal,
				&client->opportunistic_acquire, NULL);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_POOLING_MODE) == 0) {
		if (instance->config.virtual_processing) {
			if (virtual_portal_set_errorf(
				    portal, KIWI_CANT_CHANGE_RUNTIME_PARAM,
				    "parameter \"%s\" cannot be changed now",
				    ctx->s1) != 0) {
				return OD_EOOM;
			}
			return OD_SKIP;
		}
	}

	return OD_OK;
}

static od_frontend_status_t build_show_str(od_client_t *client,
					   od_virtual_portal_t *portal,
					   const char *name, const char *value)
{
	(void)client;

	if (virtual_portal_set_str_column(portal, name, value) != 0) {
		return OD_EOOM;
	}

	if (virtual_portal_set_notice(portal) != 0) {
		return OD_EOOM;
	}
	if (virtual_portal_set_complete(portal, "SHOW") != 0) {
		return OD_EOOM;
	}
	return OD_SKIP;
}

static od_frontend_status_t process_show_tsa(od_client_t *client,
					     od_virtual_portal_t *portal)
{
	od_target_session_attrs_t tsa = od_tsa_get_effective(client);
	const char *val = od_target_session_attrs_to_str(tsa);

	return build_show_str(client, portal, ODYSSEY_TARGET_SESSION_ATTRS_STR,
			      val);
}

static od_frontend_status_t
process_show_execute_on_host(od_client_t *client, od_virtual_portal_t *portal)
{
	char addr[256] = "";

	if (client->execute_on_host.host != NULL) {
		od_address_to_str(&client->execute_on_host, addr,
				  sizeof(addr) - 1);
	}

	return build_show_str(client, portal, ODYSSEY_EXECUTE_ON_HOST_STR,
			      addr);
}

static od_frontend_status_t process_show_bool_guc(od_client_t *client,
						  od_virtual_portal_t *portal,
						  const char *name, bool val)
{
	return build_show_str(client, portal, name, val ? "on" : "off");
}

static od_frontend_status_t
process_show_pooling_mode(od_client_t *client, od_virtual_portal_t *portal)
{
	const char *mode;

	switch (client->rule->pool->pool_type) {
	case OD_RULE_POOL_SESSION:
		mode = "session";
		break;
	case OD_RULE_POOL_TRANSACTION:
		mode = "transaction";
		break;
	case OD_RULE_POOL_STATEMENT:
		mode = "statement";
		break;
	default:
		mode = "unknown";
		break;
	}

	return build_show_str(client, portal, ODYSSEY_POOLING_MODE, mode);
}

static od_frontend_status_t process_show_version(od_client_t *client,
						 od_virtual_portal_t *portal)
{
	char data[128];
	od_format_version_string(data, sizeof(data));
	return build_show_str(client, portal, ODYSSEY_VERSION_STR, data);
}

static od_frontend_status_t process_vshow(od_client_t *client,
					  const od_query_ctx_t *ctx,
					  od_virtual_portal_t *portal)
{
	od_instance_t *instance = client->global->instance;

	if (strcmp(ctx->s1, ODYSSEY_TARGET_SESSION_ATTRS_STR) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_tsa(client, portal);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_EXECUTE_ON_HOST_STR) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_execute_on_host(client, portal);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_PIN_BACKEND) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_bool_guc(client, portal, ctx->s1,
						     client->backend_pin);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_RWR_CONSISTENT) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_bool_guc(client, portal, ctx->s1,
						     client->rwr_consistent);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_OPPORTUNISTIC_ACQUIRE) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_bool_guc(
				client, portal, ctx->s1,
				client->opportunistic_acquire);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_POOLING_MODE) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_pooling_mode(client, portal);
		}
	}

	if (strcmp(ctx->s1, ODYSSEY_VERSION_STR) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_version(client, portal);
		}
	}

	return OD_OK;
}

static od_frontend_status_t process_vdeallocate(od_client_t *client,
						const od_query_ctx_t *ctx,
						od_virtual_portal_t *portal)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;
	const char *command = NULL;

	if (od_query_ctx_has(ctx, OD_QUERY_CTX_IS_DEALLOCATE_ALL)) {
		od_debug(&instance->logger, "main", client, server,
			 "DEALLOCATE ALL detected, remove from client hashmap");
		od_client_pstmts_clear(client);
		od_client_portals_clear(client);

		command = "DEALLOCATE ALL";
	} else {
		od_assert(od_query_ctx_has(ctx,
					   OD_QUERY_CTX_HAS_DEALLOCATE_NAME));
		od_debug(&instance->logger, "main", client, server,
			 "DEALLOCATE '%s' detected, remove from client hashmap",
			 ctx->s1);
		od_client_remove_pstmt(client, ctx->s1);

		command = "DEALLOCATE";
	}

	if (virtual_portal_set_complete(portal, command) != 0) {
		return OD_EOOM;
	}
	return OD_SKIP;
}

static od_frontend_status_t process_vbegin(od_client_t *client,
					   od_virtual_portal_t *portal)
{
	od_server_t *server = client->server;
	int in_tx = (server != NULL && server->is_transaction);

	if (client->pending_begin || in_tx) {
		/* pass to pg and let it generate warning/error itself */
		return OD_OK;
	}

	if (virtual_portal_set_complete(portal, "BEGIN") != 0) {
		return OD_EOOM;
	}
	portal->txstatus_force = 'T';
	client->pending_begin = 1;

	return OD_SKIP;
}

/*
 * The single client write of the virtual query processing: composes
 * the wire answers from the od_virtual_portal_t that the builder
 * filled, and writes them to the client. Nothing below this function
 * ever touches the client io.
 */
static od_frontend_status_t write_virtual_portal(od_client_t *client,
						 od_virtual_portal_t *portal)
{
	od_server_t *server = client->server;

	uint8_t txstatus = 'I';
	if (server != NULL) {
		txstatus = server->is_transaction ? 'T' : 'I';
	}
	if (portal->txstatus_force) {
		txstatus = portal->txstatus_force;
	}

	/*
	 * od_write takes the message ownership (frees it after the
	 * write), so every field has to be detached right after its
	 * write - virtual_portal_free() must not see it anymore
	 */
	if (portal->notice != NULL) {
		int rc = od_write(&client->io, portal->notice);
		portal->notice = NULL;
		if (rc != 0) {
			return OD_ECLIENT_WRITE;
		}
	}

	if (portal->tts != NULL) {
		if (machine_msg_size(portal->tts->row_description) > 0) {
			int rc = od_write(&client->io,
					  portal->tts->row_description);
			portal->tts->row_description = NULL;
			if (rc != 0) {
				return OD_ECLIENT_WRITE;
			}
		}

		int rc = od_write(&client->io, portal->tts->data_rows);
		portal->tts->data_rows = NULL;
		if (rc != 0) {
			return OD_ECLIENT_WRITE;
		}
	}

	if (virtual_portal_is_error(portal)) {
		/* the ErrorResponse is already assembled on the reject path */
		int rc = od_write(&client->io, portal->error);
		portal->error = NULL;
		if (rc != 0) {
			return OD_ECLIENT_WRITE;
		}
	} else {
		/* so is the CommandComplete */
		int rc = od_write(&client->io, portal->command_complete);
		portal->command_complete = NULL;
		if (rc != 0) {
			return OD_ECLIENT_WRITE;
		}
	}

	machine_msg_t *tail = machine_msg_create(0);
	if (tail == NULL) {
		return OD_EOOM;
	}

	machine_msg_t *msg = kiwi_be_write_ready(tail, txstatus);
	if (msg == NULL) {
		machine_msg_free(tail);
		return OD_EOOM;
	}

	int rc = od_write(&client->io, tail);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_SKIP;
}

/*
 * does the query have a virtual handler at all, and is its config gate
 * open: SHOW/SET need virtual_processing (a SET may also go the
 * application_name_add_host rewrite way), BEGIN needs
 * virtual_transaction, DEALLOCATE needs reserve_prepared_statement
 */
static bool virtual_query_supported(od_client_t *client,
				    const od_query_ctx_t *ctx)
{
	od_instance_t *instance = od_global_get_instance();

	if (od_query_ctx_has(ctx, OD_QUERY_CTX_IS_SHOW)) {
		return instance->config.virtual_processing;
	}

	if (od_query_ctx_has(ctx, OD_QUERY_CTX_IS_SET)) {
		return client->rule->application_name_add_host ||
		       instance->config.virtual_processing;
	}

	if (od_query_ctx_has(ctx, OD_QUERY_CTX_IS_BEGIN)) {
		return instance->config.virtual_transaction;
	}

	if (od_query_ctx_has(ctx, OD_QUERY_CTX_IS_DEALLOCATE_ALL) ||
	    od_query_ctx_has(ctx, OD_QUERY_CTX_HAS_DEALLOCATE_NAME)) {
		return client->rule->pool->reserve_prepared_statement;
	}

	return false;
}

od_frontend_status_t try_virtual_process_query(od_client_t *client,
					       const od_query_ctx_t *ctx)
{
	if (od_query_ctx_has(ctx, OD_QUERY_CTX_PARSE_ERROR)) {
		return OD_OK;
	}

	if (!virtual_query_supported(client, ctx)) {
		return OD_OK;
	}

	od_frontend_status_t status;
	od_virtual_portal_t portal;
	virtual_portal_init(&portal);

	if (od_query_ctx_has(ctx, OD_QUERY_CTX_IS_SHOW)) {
		status = process_vshow(client, ctx, &portal);
	} else if (od_query_ctx_has(ctx, OD_QUERY_CTX_IS_SET)) {
		status = process_vset(client, ctx, &portal);
	} else if (od_query_ctx_has(ctx, OD_QUERY_CTX_IS_BEGIN)) {
		status = process_vbegin(client, &portal);
	} else {
		status = process_vdeallocate(client, ctx, &portal);
	}

	if (status == OD_SKIP) {
		/* the single client write of the virtual processing */
		status = write_virtual_portal(client, &portal);
	}

	virtual_portal_free(&portal);

	return status;
}
