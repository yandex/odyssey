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
	mm_vector_clear(&xbuf->msgs);
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

	return mm_vector_append(&xbuf->msgs, &xmsg);
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
}

void od_relay_destroy(od_relay_t *relay)
{
	od_xplan_destroy(&relay->xplan);
	xbuf_destroy(&relay->xbuf);
}

static od_frontend_status_t process_virtual_set(od_client_t *client,
						od_parser_t *parser)
{
	od_token_t token;
	od_keyword_t *keyword;
	char *option_value;
	int option_value_len;
	int rc;
	od_server_t *server;
	od_instance_t *instance = client->global->instance;

	server = client->server;

	/* need to read exact odyssey parameter here */
	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_SYMBOL || token.value.num != '.') {
		return OD_OK;
	}

	rc = od_parser_next(parser, &token);
	switch (rc) {
	case OD_PARSER_KEYWORD:
		keyword = od_keyword_match(od_query_process_keywords, &token);
		if (keyword == NULL ||
		    keyword->id != OD_QUERY_PROCESSING_LTSA) {
			/* some other option, skip */
			return OD_OK;
		}
		break;
	default:
		return OD_OK;
	}

	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_SYMBOL || token.value.num != '=') {
		return OD_OK;
	}

	rc = od_parser_next(parser, &token);

	if (rc != OD_PARSER_STRING) {
		return OD_OK;
	}

	option_value = token.value.string.pointer;
	option_value_len = token.value.string.size;

	/* for now, very straightforward logic, as there is only one supported param */
	if (strncasecmp(option_value, "read-only", option_value_len) == 0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      option_value, option_value_len);
	} else if (strncasecmp(option_value, "read-write", option_value_len) ==
		   0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      option_value, option_value_len);
	} else if (strncasecmp(option_value, "any", option_value_len) == 0) {
		kiwi_vars_set(&client->vars,
			      KIWI_VAR_ODYSSEY_TARGET_SESSION_ATTRS,
			      option_value, option_value_len);
	} else {
		/* some other option name, fallback to regular logic */
		return OD_OK;
	}

	od_debug(&instance->logger, "virtual processing", client, server,
		 "parsed tsa hint %.*s", option_value_len, option_value);

	machine_msg_t *msg =
		kiwi_be_write_command_complete(NULL, "SET", sizeof("SET"));
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

static od_frontend_status_t process_set_appname(od_client_t *client,
						od_parser_t *parser)
{
	int rc;
	od_token_t token;

	rc = od_parser_next(parser, &token);
	switch (rc) {
	/* set application_name to ... */
	case OD_PARSER_KEYWORD: {
		od_keyword_t *keyword =
			od_keyword_match(od_query_process_keywords, &token);
		if (keyword == NULL || keyword->id != OD_QUERY_PROCESSING_LTO) {
			goto error;
		}
		break;
	}

	/* set application_name = ... */
	case OD_PARSER_SYMBOL:
		if (token.value.num != '=') {
			goto error;
		}
		break;

	default:
		goto error;
	}

	/* read original appname */
	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_STRING) {
		goto error;
	}

	char original_appname[64];
	size_t len =
		(size_t)token.value.string.size > sizeof(original_appname) ?
			sizeof(original_appname) :
			(size_t)token.value.string.size;
	strncpy(original_appname, token.value.string.pointer, len);

	/* query should end with ; */
	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_SYMBOL || token.value.num != ';') {
		goto error;
	}

	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_EOF) {
		goto error;
	}

	char peer_name[KIWI_MAX_VAR_SIZE];
	rc = od_getpeername(client->io.io, peer_name, sizeof(peer_name), 1, 0);
	if (rc != 0) {
		od_gerror("query", client, client->server,
			  "can't get peer name, errno = ", machine_errno());
		goto error;
	}

	/*
	 * done parsing - need to replace the message
	 */

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
					 od_parser_t *parser)
{
	od_instance_t *instance = od_global_get_instance();
	int rc;
	od_token_t token;
	od_keyword_t *keyword;

	/* need to read attribute name that are setting */
	rc = od_parser_next(parser, &token);
	switch (rc) {
	case OD_PARSER_KEYWORD:
		keyword = od_keyword_match(od_query_process_keywords, &token);
		if (keyword == NULL) {
			/* some other option, skip */
			return OD_OK;
		}

		if (keyword->id == OD_QUERY_PROCESSING_LAPPNAME) {
			/* this is set application_name ... query */
			if (client->rule->application_name_add_host) {
				return process_set_appname(client, parser);
			}
		}

		if (keyword->id == OD_QUERY_PROCESSING_LODYSSEY) {
			/*
			* this is odyssey-specific virtual values set like
			* set odyssey.target_session_attr = 'read-only';
			*/
			if (instance->config.virtual_processing) {
				int retstatus =
					process_virtual_set(client, parser);
				if (retstatus != OD_OK) {
					return retstatus;
				}
			}
		}
		break;
	default:
		return OD_OK;
	}

	return OD_OK;
}

static od_frontend_status_t
try_virtual_process_query(od_client_t *client, char *query, uint32_t query_len)
{
	od_instance_t *instance = od_global_get_instance();

	int need_process = client->rule->application_name_add_host ||
			   instance->config.virtual_processing;
	if (!need_process) {
		return OD_OK;
	}

	int rc;
	od_parser_t parser;
	od_parser_init_queries_mode(&parser, query,
				    query_len - 1 /* len is zero included */);

	od_token_t token;
	rc = od_parser_next(&parser, &token);

	/* all processed queries starts with show or set now */
	if (rc != OD_PARSER_KEYWORD) {
		return OD_OK;
	}

	od_keyword_t *keyword;
	keyword = od_keyword_match(od_query_process_keywords, &token);

	if (keyword == NULL) {
		return OD_OK;
	}

	switch (keyword->id) {
	case OD_QUERY_PROCESSING_LSET:
		return process_vset(client, &parser);
	case OD_QUERY_PROCESSING_LSHOW:
	/* fallthrough */
	/* XXX: implement virtual show */
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
		assert(client->server != NULL);

		/* to process the message, server was acquired, try again */
		status = handler(relay, msg, timeout_ms);
		assert(status != OD_ATTACH);
	}

	return status;
}

static od_frontend_status_t
process_query_impl(od_relay_t *relay, machine_msg_t *msg, uint32_t timeout_ms)
{
	od_client_t *client = relay->client;
	od_instance_t *instance = client->global->instance;
	od_route_t *route = client->route;
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

	if (query_len >= 7 && route->rule->pool->reserve_prepared_statement) {
		if (strncmp(query, "DISCARD", 7) == 0) {
			od_debug(&instance->logger, "simple query", client,
				 server, "discard detected, invalidate caches");
			od_client_pstmts_clear(client);
			od_server_pstmts_clear(server);
		}
	}

	status = try_virtual_process_query(client, query, query_len);
	if (status == OD_SKIP) {
		/* query must not be sent to backend */
		return OD_OK;
	} else if (status == OD_REPLACED) {
		/* replaced query was sent, wait rfq but no writing the query */
	} else if (status == OD_OK) {
		if (server == NULL) {
			return OD_ATTACH;
		}

		rc = od_io_write(&server->io, msg, timeout_ms);
		if (rc != 0) {
			return OD_ESERVER_WRITE;
		}

		od_server_sync_request(server, 1);
	} else {
		/* not skip, not replace and not ok - need handle on higher level */
		return status;
	}

	od_stat_query_start(&server->stats_state);

	status = od_stream_server_until_rfq("main", server, timeout_ms);

	if (status == OD_COPY_IN_RECEIVED) {
		/*
		 * server is awaiting CopyData from client - stream it
		 */
		status = od_stream_copy_to_server("main", client, server,
						  timeout_ms);
	}

	return status;
}

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

	if (xbuf_append(&relay->xbuf, msg)) {
		return OD_EOOM;
	}

	status = od_xplan_make_from_xbuf(&relay->xplan, relay);
	if (status != OD_OK) {
		return status;
	}

	status = od_xplan_run(&relay->xplan, relay, timeout_ms);

	/* never reuse this ones */
	xbuf_clear(&relay->xbuf);
	od_xplan_clear(&relay->xplan);

	return status;
}

static od_frontend_status_t
process_fcall_impl(od_relay_t *relay, machine_msg_t *msg, uint32_t timeout_ms)
{
	od_client_t *client = relay->client;
	od_server_t *server = client->server;

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

	/*
	 * in vanilla PG, executing simple query removes the
	 * unnamed pstmt on client
	 */
	if (relay->client->prep_stmt_ids != NULL) {
		od_client_remove_pstmt(relay->client, "");
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
	return process_possible_attach(execute_xbuf, relay, msg, timeout_ms);
}

od_frontend_status_t od_relay_process_xsync(od_relay_t *relay,
					    machine_msg_t *msg,
					    uint32_t timeout_ms)
{
	return process_possible_attach(execute_xbuf, relay, msg, timeout_ms);
}

od_frontend_status_t od_relay_process_xmsg(od_relay_t *relay,
					   machine_msg_t *msg,
					   uint32_t timeout_ms)
{
	(void)timeout_ms;

	/* no any handling - real queries will be executed on flush/sync */

	if (xbuf_append(&relay->xbuf, msg)) {
		return OD_EOOM;
	}

	return OD_OK;
}
