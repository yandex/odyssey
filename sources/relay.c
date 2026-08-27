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
#include <parser.h>
#include <query_processing.h>
#include <stream.h>
#include <xplan.h>
#include <pstmt.h>
#include <misc.h>
#include <worker.h>
#include <sql/minimal/parser.h>

#define APPLICATION_NAME_STR "application_name"
#define ODYSSEY_TARGET_SESSION_ATTRS_STR "odyssey.target_session_attrs"
#define ODYSSEY_PIN_BACKEND "odyssey.pin_backend"
#define PROCESSED_BY_ODYSSEY_STR "processed virtually by odyssey"

/*
 * relay - client messages handling subsystem
 * made generally for extended protocol
 *
 * simple protocol is handled in straight
 *
 * extended protocol is handled by putting the messages
 * in buffer, until Flush or Sync messages are met
 *
 * on flush and sync msgs the buffer are transformed
 * into plan, which is the msg sequence with inserted
 * Prepare msgs and virtual responses
 */

static void xbuf_msg_destroy(void *a)
{
	od_xbuf_msg_t *m = a;
	machine_msg_free_safe(m->msg);
}

static void xbuf_init(od_relay_xbuf_t *xbuf)
{
	mm_vector_init(&xbuf->msgs, sizeof(od_xbuf_msg_t), xbuf_msg_destroy);
}

static void xbuf_destroy_msgs(od_relay_xbuf_t *xbuf)
{
	/* TODO: do not ignore error? */
	mm_vector_clear(&xbuf->msgs);
	mm_vector_shrink_to_fit(&xbuf->msgs);
}

static void xbuf_destroy(od_relay_xbuf_t *xbuf)
{
	mm_vector_destroy(&xbuf->msgs);
}

static int xbuf_append(od_relay_xbuf_t *xbuf, machine_msg_t *msg)
{
	machine_msg_t *copy = machine_msg_copy(msg);
	if (copy == NULL) {
		return -1;
	}

	od_xbuf_msg_t xmsg;
	memset(&xmsg, 0, sizeof(od_xbuf_msg_t));
	xmsg.msg = copy;

	int rc = mm_vector_append(&xbuf->msgs, &xmsg);
	if (rc != 0) {
		machine_msg_free(copy);
	}

	return rc;
}

static int xbuf_append_raw(od_relay_xbuf_t *xbuf, const void *data, size_t len)
{
	machine_msg_t *copy = machine_msg_create(len);
	if (copy == NULL) {
		return -1;
	}

	char *d = machine_msg_data(copy);
	memcpy(d, data, len);

	od_xbuf_msg_t xmsg;
	memset(&xmsg, 0, sizeof(od_xbuf_msg_t));
	xmsg.msg = copy;

	int rc = mm_vector_append(&xbuf->msgs, &xmsg);
	if (rc != 0) {
		machine_msg_free(copy);
	}

	return rc;
}

static void xbuf_clear(od_relay_xbuf_t *xbuf)
{
	xbuf_destroy_msgs(xbuf);
}

void od_relay_init(od_relay_t *relay, od_client_t *client)
{
	xbuf_init(&relay->xbuf);
	od_xplan_init(&relay->xplan);
	relay->client = client;

	relay->copy_additional = NULL;
}

void od_relay_destroy(od_relay_t *relay)
{
	od_xplan_destroy(&relay->xplan);
	xbuf_destroy(&relay->xbuf);
}

static od_frontend_status_t reply_reject_guc(od_client_t *client,
					     const char *guc_name,
					     const char *option_value,
					     size_t option_value_len)
{
	od_server_t *server;

	server = client->server;

	machine_msg_t *m =
		kiwi_be_write_notice(NULL, 'M', PROCESSED_BY_ODYSSEY_STR);
	if (m == NULL) {
		return OD_EOOM;
	}

	char buf[128];
	int len = od_snprintf(buf, sizeof(buf),
			      "invalid value for parameter \"%s\": \"%.*s\"",
			      guc_name, (int)option_value_len, option_value);

	m = kiwi_be_write_error(m, KIWI_INVALID_PARAMETER_VALUE, buf, len);
	if (m == NULL) {
		return OD_EOOM;
	}

	int rc;
	rc = od_write2(&client->io, m, 1000);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	uint8_t txstatus = 'I';
	if (server != NULL) {
		txstatus = server->is_transaction ? 'T' : 'I';
	}

	m = kiwi_be_write_ready(NULL, txstatus);

	rc = od_write2(&client->io, m, 1000);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_SKIP;
}

static od_frontend_status_t
process_set_generic_bool(od_client_t *client,
			 const od_sql_minimal_set_stmt_t *stmt,
			 bool *guc_val_ptr)
{
	const char *option_value;
	size_t option_value_len;

	od_server_t *server;
	od_instance_t *instance = client->global->instance;

	server = client->server;
	option_value = stmt->value;
	option_value_len = strlen(option_value);

	if (strncasecmp(option_value, "true", option_value_len) == 0 ||
	    strncasecmp(option_value, "on", option_value_len) == 0 ||
	    strncasecmp(option_value, "1", option_value_len) == 0) {
		*guc_val_ptr = 1;
	} else if (strncasecmp(option_value, "false", option_value_len) == 0 ||
		   strncasecmp(option_value, "off", option_value_len) == 0 ||
		   strncasecmp(option_value, "0", option_value_len) == 0) {
		*guc_val_ptr = 0;
	} else {
		/* Reject this */
		return reply_reject_guc(client, stmt->key, option_value,
					option_value_len);
	}

	/* XXX: refactor this */
	od_debug(&instance->logger, "virtual processing", client, server,
		 "processed virtual bool GUC %.*s", (int)option_value_len,
		 option_value);

	uint8_t txstatus = 'I';
	if (server != NULL) {
		txstatus = server->is_transaction ? 'T' : 'I';
	}

	char msg[128 /* message below is ~ 60 bytes */];
	int rc;
	char *out = msg;
	char *end = msg + sizeof(msg);
	rc = kiwi_be_format_notice(out, end - out, 'M',
				   PROCESSED_BY_ODYSSEY_STR);
	od_assert(rc != -1);
	out += rc;

	rc = kiwi_be_format_command_complete(out, end - out, "SET");
	od_assert(rc != -1);
	out += rc;

	rc = kiwi_be_format_ready(out, end - out, txstatus);
	od_assert(rc != -1);
	out += rc;

	size_t unused;
	rc = od_io_write_raw(&client->io, msg, out - msg, &unused, 1000, 0);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_SKIP;
}

static od_frontend_status_t
process_set_tsa(od_client_t *client, const od_sql_minimal_set_stmt_t *stmt)
{
	const char *option_value;
	size_t option_value_len;
	od_server_t *server;
	od_instance_t *instance = client->global->instance;

	server = client->server;

	option_value = stmt->value;
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

		return reply_reject_guc(client,
					ODYSSEY_TARGET_SESSION_ATTRS_STR,
					option_value, option_value_len);
	}

	od_debug(&instance->logger, "virtual processing", client, server,
		 "parsed tsa hint %.*s", (int)option_value_len, option_value);

	uint8_t txstatus = 'I';
	if (server != NULL) {
		txstatus = server->is_transaction ? 'T' : 'I';
	}

	char msg[128 /* message below is ~ 60 bytes */];
	int rc;
	char *out = msg;
	char *end = msg + sizeof(msg);
	rc = kiwi_be_format_notice(out, end - out, 'M',
				   PROCESSED_BY_ODYSSEY_STR);
	od_assert(rc != -1);
	out += rc;

	rc = kiwi_be_format_command_complete(out, end - out, "SET");
	od_assert(rc != -1);
	out += rc;

	rc = kiwi_be_format_ready(out, end - out, txstatus);
	od_assert(rc != -1);
	out += rc;

	size_t unused;
	rc = od_io_write_raw(&client->io, msg, out - msg, &unused, 1000, 0);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_SKIP;
}

static od_frontend_status_t
process_set_appname(od_client_t *client, const od_sql_minimal_set_stmt_t *stmt)
{
	int rc;
	char original_appname[64];
	size_t len = od_min(strlen(stmt->value), sizeof(original_appname));
	snprintf(original_appname, sizeof(original_appname), "%s", stmt->value);

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
	od_snprintf(query, sizeof(query), "set application_name to '%.*s';",
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

static od_frontend_status_t process_vset(od_client_t *client,
					 const od_sql_minimal_set_stmt_t *stmt)
{
	od_instance_t *instance = od_global_get_instance();

	if (strcmp(stmt->key, APPLICATION_NAME_STR) == 0) {
		if (client->rule->application_name_add_host) {
			return process_set_appname(client, stmt);
		}
	}

	if (strcmp(stmt->key, ODYSSEY_TARGET_SESSION_ATTRS_STR) == 0) {
		if (instance->config.virtual_processing) {
			return process_set_tsa(client, stmt);
		}
	}

	if (strcmp(stmt->key, ODYSSEY_PIN_BACKEND) == 0) {
		if (instance->config.virtual_processing) {
			return process_set_generic_bool(client, stmt,
							&client->backend_pin);
		}
	}

	return OD_OK;
}

static od_frontend_status_t virtual_str_ans(od_client_t *client,
					    const char *name, const char *value)
{
	machine_msg_t *stream = NULL;
	machine_msg_t *msg = NULL;
	int rc;
	int offset;
	uint8_t txstatus;
	od_server_t *server = client->server;

	stream = machine_msg_create(0);
	if (stream == NULL) {
		goto fail_oom;
	}

	msg = kiwi_be_write_notice(stream, 'M', PROCESSED_BY_ODYSSEY_STR);
	if (msg == NULL) {
		goto fail_oom;
	}

	msg = kiwi_be_write_row_descriptionf(stream, "s", name);
	if (msg == NULL) {
		goto fail_oom;
	}

	msg = kiwi_be_write_data_row(stream, &offset);
	if (msg == NULL) {
		goto fail_oom;
	}

	rc = kiwi_be_write_data_row_add(stream, offset, value, strlen(value));
	if (rc != 0) {
		goto fail_oom;
	}

	rc = kiwi_be_write_complete(stream, "SHOW", 5);
	if (rc != 0) {
		goto fail_oom;
	}

	txstatus = 'I';
	if (server != NULL) {
		txstatus = server->is_transaction ? 'T' : 'I';
	}

	msg = kiwi_be_write_ready(stream, txstatus);
	if (msg == NULL) {
		goto fail_oom;
	}

	rc = od_write(&client->io, stream);
	if (rc != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_SKIP;

fail_oom:
	machine_msg_free(stream);
	return OD_EOOM;
}

static od_frontend_status_t process_show_tsa(od_client_t *client)
{
	od_target_session_attrs_t tsa = od_tsa_get_effective(client);
	const char *val = od_target_session_attrs_to_str(tsa);

	return virtual_str_ans(client, ODYSSEY_TARGET_SESSION_ATTRS_STR, val);
}

static od_frontend_status_t process_show_bool_guc(od_client_t *client,
						  const char *name, bool val)
{
	return virtual_str_ans(client, name, val ? "on" : "off");
}

static od_frontend_status_t
process_vshow(od_client_t *client, const od_sql_minimal_show_stmt_t *stmt)
{
	od_instance_t *instance = od_global_get_instance();

	if (strcmp(stmt->name, ODYSSEY_TARGET_SESSION_ATTRS_STR) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_tsa(client);
		}
	}

	if (strcmp(stmt->name, ODYSSEY_PIN_BACKEND) == 0) {
		if (instance->config.virtual_processing) {
			return process_show_bool_guc(client, stmt->name,
						     client->backend_pin);
		}
	}

	return OD_OK;
}

static od_frontend_status_t
process_vdeallocate(od_client_t *client,
		    const od_sql_minimal_deallocate_stmt_t *deallocate)
{
	od_instance_t *instance = client->global->instance;
	od_server_t *server = client->server;
	const char *command = NULL;

	if (deallocate->is_all) {
		od_debug(&instance->logger, "main", client, server,
			 "DEALLOCATE ALL detected, remove from client hashmap");
		od_client_pstmts_clear(client);
		od_client_portals_clear(client);

		command = "DEALLOCATE ALL";
	} else {
		od_assert(deallocate->name != NULL);
		od_debug(&instance->logger, "main", client, server,
			 "DEALLOCATE '%s' detected, remove from client hashmap",
			 deallocate->name);
		od_client_remove_pstmt(client, deallocate->name);

		command = "DEALLOCATE";
	}

	machine_msg_t *msg = kiwi_be_write_command_complete(NULL, command);
	if (msg == NULL) {
		return OD_EOOM;
	}

	uint8_t txstatus = 'I';
	if (server != NULL) {
		txstatus = server->is_transaction ? 'T' : 'I';
	}

	msg = kiwi_be_write_ready(msg, txstatus);
	if (msg == NULL) {
		return OD_EOOM;
	}

	if (od_write(&client->io, msg) != 0) {
		return OD_ECLIENT_WRITE;
	}

	return OD_SKIP;
}

static od_frontend_status_t
process_vbegin(od_client_t *client, const od_sql_minimal_begin_stmt_t *stmt)
{
	(void)stmt;

	od_server_t *server = client->server;
	int in_tx = (server != NULL && server->is_transaction);

	if (client->pending_begin || in_tx) {
		/* pass to pg and let it generate warning/error itself */
		return OD_OK;
	}

	machine_msg_t *msg = kiwi_be_write_command_complete(NULL, "BEGIN");
	if (msg == NULL) {
		return OD_EOOM;
	}

	msg = kiwi_be_write_ready(msg, 'T');
	if (msg == NULL) {
		return OD_EOOM;
	}

	if (od_write(&client->io, msg) != 0) {
		return OD_ECLIENT_WRITE;
	}

	client->pending_begin = 1;

	return OD_SKIP;
}

static od_frontend_status_t try_virtual_process_query(od_client_t *client,
						      od_linear_alloc_t *arena,
						      const char *query,
						      uint32_t query_len)
{
	od_instance_t *instance = od_global_get_instance();
	int need_process;

	if (instance->config.query_parsing.mode ==
	    OD_CONFIG_QUERY_PARSING_MODE_DISABLED) {
		return OD_OK;
	}

	od_sql_minimal_node_t *ast = od_sql_minimal_parse(
		query, query_len - 1 /* zero included */, arena, NULL, NULL);
	if (ast == NULL) {
		return OD_OK;
	}

	switch (ast->type) {
	case OD_SQL_MINIMAL_NODE_TYPE_SHOW_STMT:
		need_process = instance->config.virtual_processing;
		if (!need_process) {
			return OD_OK;
		}

		return process_vshow(client,
				     (const od_sql_minimal_show_stmt_t *)ast);
		return OD_OK;
	case OD_SQL_MINIMAL_NODE_TYPE_SET_STMT:
		need_process = client->rule->application_name_add_host ||
			       instance->config.virtual_processing;
		if (!need_process) {
			return OD_OK;
		}

		return process_vset(client,
				    (const od_sql_minimal_set_stmt_t *)ast);
	case OD_SQL_MINIMAL_NODE_TYPE_BEGIN_STMT:
		need_process = instance->config.virtual_transaction;
		if (!need_process) {
			return OD_OK;
		}

		return process_vbegin(client,
				      (const od_sql_minimal_begin_stmt_t *)ast);
	case OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT:
		need_process = client->rule->pool->reserve_prepared_statement;
		if (!need_process) {
			return OD_OK;
		}

		return process_vdeallocate(
			client, (const od_sql_minimal_deallocate_stmt_t *)ast);
	case OD_SQL_MINIMAL_NODE_TYPE_DISCARD_STMT:
		client->query_ctx.is_discard_all =
			(((const od_sql_minimal_discard_stmt_t *)ast)->target ==
			 OD_SQL_MINIMAL_DISCARD_ALL);
		/* fallthrough */
	default:
		return OD_OK;
	}
}

typedef od_frontend_status_t (*handler_t)(od_relay_t *relay, machine_msg_t *msg,
					  uint32_t timeout_ms);

static od_frontend_status_t process_possible_attach(handler_t handler,
						    od_relay_t *relay,
						    machine_msg_t *msg,
						    uint32_t timeout_ms)
{
	od_frontend_status_t status = handler(relay, msg, timeout_ms);
	if (status == OD_ATTACH) {
		od_client_t *client = relay->client;
		status = od_frontend_attach_and_deploy(client, "main");
		if (status != OD_OK) {
			return status;
		}
		od_assert(client->server != NULL);

		/* to process the message, server was acquired, try again */
		status = handler(relay, msg, timeout_ms);
		od_assert(status != OD_ATTACH);
	}

	return status;
}

static void process_discard(od_client_t *client, od_server_t *server)
{
	od_route_t *route = client->route;
	od_instance_t *instance = client->global->instance;

	if (!route->rule->pool->reserve_prepared_statement) {
		return;
	}

	if (client->query_ctx.is_discard_all) {
		od_debug(&instance->logger, "main", client, server,
			 "DISCARD ALL detected, invalidate caches");

		od_client_pstmts_clear(client);
		od_client_portals_clear(client);
		if (server != NULL) {
			od_server_pstmts_clear(server);
		}
	}
}

static od_frontend_status_t proxy_until_command_complete(od_client_t *client,
							 od_server_t *server,
							 uint32_t timeout_ms)
{
	int rc;
	int done = 0;
	machine_msg_t *msg = NULL;

	while (!done) {
		msg = od_read(&server->io, timeout_ms, OD_READ_BE);
		if (msg == NULL) {
			return OD_ESERVER_READ;
		}

		char *data = machine_msg_data(msg);
		int size = machine_msg_size(msg);
		kiwi_be_type_t type = *data;
		od_instance_t *instance = client->global->instance;

		if (instance->config.log_debug) {
			if (type == KIWI_BE_COMMAND_COMPLETE) {
				const char *command_tag =
					data + sizeof(kiwi_header_t);
				od_debug(&instance->logger, "main", client,
					 server, "%s - %s",
					 kiwi_be_type_to_string(type),
					 command_tag);
			} else {
				od_debug(&instance->logger, "main", client,
					 server, "%s",
					 kiwi_be_type_to_string(type));
			}
		}

		switch (type) {
		case KIWI_BE_COMMAND_COMPLETE:
			done = 1;
			machine_msg_free(msg);
			break;
		case KIWI_BE_PARAMETER_STATUS:
			rc = od_backend_update_parameter(server, "main", data,
							 size, 0);
			if (rc == -1) {
				machine_msg_free(msg);
				od_gerror("main", client, server,
					  "can't update parameter");
				return OD_ESERVER_READ;
			}
			/* fallthrough */
		default:
			rc = od_write(&client->io, msg);
			if (rc != 0) {
				return OD_ECLIENT_READ;
			}
			break;
		}
	}

	return OD_OK;
}

static od_frontend_status_t
process_query_impl(od_relay_t *relay, machine_msg_t *msg, uint32_t timeout_ms)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;
	od_frontend_status_t status;

	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);
	int rc;

	char *query;
	uint32_t query_len;
	rc = kiwi_be_read_query(data, size, &query, &query_len);
	if (rc != OK_RESPONSE) {
		od_gerror("main", client, server, "can't parse query message");
		return OD_ESERVER_WRITE;
	}

	od_linear_alloc_t *arena = od_worker_get_local_linear_alloc();
	status = try_virtual_process_query(client, arena, query, query_len);
	od_linear_alloc_reset(arena);

	if (status == OD_SKIP) {
		/* query must not be sent to backend */
		return OD_OK;
	} else if (status == OD_REPLACED) {
		/* replaced query was sent, wait rfq but no writing the query */
	} else if (status == OD_OK) {
		if (server == NULL) {
			return OD_ATTACH;
		}

		if (client->pending_begin) {
			machine_msg_t *rewritten = kiwi_fe_write_query_join(
				NULL, "BEGIN;", query, NULL);
			if (rewritten == NULL) {
				return OD_EOOM;
			}

			rc = od_io_write(&server->io, rewritten, timeout_ms);
			machine_msg_free(rewritten);
		} else {
			rc = od_io_write(&server->io, msg, timeout_ms);
		}

		if (rc != 0) {
			return OD_ESERVER_WRITE;
		}

		od_server_sync_request(server, 1);
	} else {
		/* not skip, not replace and not ok - need handle on higher level */
		return status;
	}

	od_stat_query_start(&server->stats_state);

	if (client->pending_begin) {
		/*
		 * need to consume CommandComplete(BEGIN)
		 * and maybe bypass some asynchronious messages
		 */
		status = proxy_until_command_complete(client, server,
						      timeout_ms);
		if (status != OD_OK) {
			return status;
		}
		client->pending_begin = 0;
	}

	status = od_stream_server_until_rfq("main", server, timeout_ms);

	if (status == OD_COPY_IN_RECEIVED) {
		/*
		 * server is awaiting CopyData from client - stream it
		 */
		machine_msg_t *add = od_relay_get_copy_additional(relay);
		od_assert(add == NULL);
		status = od_stream_copy_to_server("main", client, server, add,
						  timeout_ms);
	}

	if (status == OD_OK) {
		/* process only after success query completion */
		process_discard(client, server);
	}

	return status;
}

uint8_t od_relay_deferred_begin_bytes[12] = { 'Q', 0,	0,   0,	  11,  'B',
					      'E', 'G', 'I', 'N', ';', 0 };

static int relay_append(od_relay_t *relay, machine_msg_t *msg)
{
	od_client_t *client = relay->client;

	if (client->pending_begin) {
		size_t len = sizeof(od_relay_deferred_begin_bytes);
		if (xbuf_append_raw(&relay->xbuf, od_relay_deferred_begin_bytes,
				    len)) {
			return -1;
		}

		client->pending_begin = 0;
	}

	if (xbuf_append(&relay->xbuf, msg)) {
		return -1;
	}

	return 0;
}

/* note: does not free the buffers */
static od_frontend_status_t execute_xbuf(od_relay_t *relay, machine_msg_t *msg,
					 uint32_t timeout_ms)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;
	od_frontend_status_t status;

	if (server == NULL) {
		/* we will write/read to/from server - attach if needed */
		return OD_ATTACH;
	}

	if (relay_append(relay, msg)) {
		return OD_EOOM;
	}

	status = od_xplan_make_from_xbuf(&relay->xplan, relay);
	if (status != OD_OK) {
		return status;
	}

	status = od_xplan_run(&relay->xplan, relay, timeout_ms);

	return status;
}

static od_frontend_status_t
process_fcall_impl(od_relay_t *relay, machine_msg_t *msg, uint32_t timeout_ms)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;

	if (server == NULL) {
		return OD_ATTACH;
	}

	/* no need special handling - just write call and wait for rfq */

	int rc = od_io_write(&server->io, msg, timeout_ms);
	if (rc != 0) {
		return OD_ESERVER_WRITE;
	}

	od_server_sync_request(server, 1);

	od_stat_query_start(&server->stats_state);

	return od_stream_server_until_rfq("main", server, timeout_ms);
}

od_frontend_status_t od_relay_process_query(od_relay_t *relay,
					    machine_msg_t *msg,
					    uint32_t timeout_ms)
{
	od_frontend_status_t status = process_possible_attach(
		process_query_impl, relay, msg, timeout_ms);

	memset(&relay->client->query_ctx, 0, sizeof(relay->client->query_ctx));

	/*
	 * in vanilla PG, executing simple query removes the
	 * unnamed pstmt and unnamed portal on client
	 */
	if (relay->client->prep_stmt_ids != NULL) {
		od_client_remove_pstmt(relay->client, "");
	}
	if (relay->client->portals != NULL) {
		od_client_remove_portal(relay->client, "");
	}

	return status;
}

od_frontend_status_t od_relay_process_fcall(od_relay_t *relay,
					    machine_msg_t *msg,
					    uint32_t timeout_ms)
{
	od_frontend_status_t status = process_possible_attach(
		process_fcall_impl, relay, msg, timeout_ms);

	return status;
}

od_frontend_status_t od_relay_process_xflush(od_relay_t *relay,
					     machine_msg_t *msg,
					     uint32_t timeout_ms)
{
	od_frontend_status_t status =
		process_possible_attach(execute_xbuf, relay, msg, timeout_ms);

	/* never reuse this ones */
	xbuf_clear(&relay->xbuf);
	od_xplan_clear(&relay->xplan);

	return status;
}

od_frontend_status_t od_relay_process_xsync(od_relay_t *relay,
					    machine_msg_t *msg,
					    uint32_t timeout_ms)
{
	od_frontend_status_t status =
		process_possible_attach(execute_xbuf, relay, msg, timeout_ms);

	/* never reuse this ones */
	xbuf_clear(&relay->xbuf);
	od_xplan_clear(&relay->xplan);

	return status;
}

od_frontend_status_t od_relay_process_xmsg(od_relay_t *relay,
					   machine_msg_t *msg,
					   uint32_t timeout_ms)
{
	(void)timeout_ms;

	/* no any handling - real queries will be executed on flush/sync */

	if (relay_append(relay, msg)) {
		return OD_EOOM;
	}

	return OD_OK;
}
