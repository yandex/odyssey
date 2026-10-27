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
#include <wait_event.h>
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
#include <virtual.h>

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

	if (m->pstmt != NULL) {
		od_pstmt_unref(m->pstmt);
	}

	machine_msg_free_safe(m->msg);
}

static inline kiwi_fe_type_t xbuf_msg_fe_type(machine_msg_t *msg)
{
	return *(uint8_t *)machine_msg_data(msg);
}

/*
 * client pstmt name (Parse) or portal name (Bind/Execute)
 * of the msg, points into the msg data
 *
 * returns NULL if the msg has no name or can not be read
 */
static const char *xbuf_msg_name(machine_msg_t *msg, kiwi_fe_type_t type)
{
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);

	char *name;
	uint32_t name_len;

	switch (type) {
	case KIWI_FE_PARSE: {
		char *query;
		uint32_t query_len;
		if (kiwi_be_read_parse(data, size, &name, &name_len, &query,
				       &query_len) != 0) {
			return NULL;
		}
		return name;
	}
	case KIWI_FE_BIND: {
		char *pstmt_name;
		uint32_t pstmt_name_len;
		if (kiwi_be_read_bind_names(data, size, &name, &name_len,
					    &pstmt_name,
					    &pstmt_name_len) != 0) {
			return NULL;
		}
		return name;
	}
	case KIWI_FE_EXECUTE:
		if (kiwi_be_read_execute(data, size, &name, &name_len) != 0) {
			return NULL;
		}
		return name;
	default:
		return NULL;
	}
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
		status = od_frontend_attach_and_deploy(client);
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

	if (od_query_ctx_has(&client->query_ctx, OD_QUERY_CTX_IS_DISCARD_ALL)) {
		od_debug(&instance->logger, client, server,
			 "DISCARD ALL detected, invalidate caches");

		od_client_pstmts_clear(client);
		od_client_portals_clear(client);
		if (server != NULL) {
			od_server_pstmts_clear(server);
		}
	}
}

static inline void process_unlisten(od_client_t *client, od_server_t *server)
{
	od_route_t *route = client->route;
	od_instance_t *instance = client->global->instance;

	if (!route->rule->pool->pin_on_listen) {
		return;
	}

	if (od_query_ctx_has(&client->query_ctx,
			     OD_QUERY_CTX_IS_UNLISTEN_ALL) &&
	    client->backend_pin_listen) {
		od_debug(&instance->logger, client, server,
			 "UNLISTEN ALL detected, unpin client");

		client->backend_pin = 0;
		client->backend_pin_listen = 0;
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
				od_debug(&instance->logger, client, server,
					 "%s - %s",
					 kiwi_be_type_to_string(type),
					 command_tag);
			} else {
				od_debug(&instance->logger, client, server,
					 "%s", kiwi_be_type_to_string(type));
			}
		}

		switch (type) {
		case KIWI_BE_COMMAND_COMPLETE:
			done = 1;
			machine_msg_free(msg);
			break;
		case KIWI_BE_PARAMETER_STATUS:
			rc = od_backend_update_parameter(server, data, size, 0);
			if (rc == -1) {
				machine_msg_free(msg);
				od_gerror(client, server,
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
		od_gerror(client, server, "can't parse query message");
		return OD_ESERVER_WRITE;
	}

	od_linear_alloc_t *arena = od_worker_get_local_linear_alloc();

	od_instance_t *instance = client->global->instance;

	od_sql_pstmt_query_t *sql_query = &client->sql_query;
	if (client->rule->pool->reserve_prepared_statement &&
	    !sql_query->analyzed) {
		/* must run first: the full parser resets the arena */
		rc = od_sql_pstmt_query_analyze(
			sql_query, query, query_len - 1 /* zero included */,
			arena);
		if (rc != 0) {
			return OD_EOOM;
		}
	}

	od_config_query_parsing_t parsing = instance->config.query_parsing;
	if (parsing.mode == OD_CONFIG_QUERY_PARSING_MODE_FULL &&
	    !od_tsa_auto_route_ro_enabled(client)) {
		/*
		 * auto routing is the only consumer of the full-parser-only
		 * results (is_select) - for clients without it the cheaper
		 * minimal parser is enough
		 */
		parsing.mode = OD_CONFIG_QUERY_PARSING_MODE_MINIMAL;
	}

	od_query_parse_fill_ctx(query, query_len - 1 /* zero included */, arena,
				&client->query_ctx, &parsing);

	status = try_virtual_process_query(client, &client->query_ctx);
	od_linear_alloc_reset(arena, 0);

	if (status == OD_SKIP) {
		/* query must not be sent to backend */
		return OD_OK;
	} else if (status == OD_REPLACED) {
		/* replaced query was sent, wait rfq but no writing the query */
	} else if (status == OD_OK) {
		if (server == NULL) {
			return OD_ATTACH;
		}

		status = od_sql_pstmt_query_rewrite(client);
		if (status != OD_OK) {
			return status;
		}
		machine_msg_t *forward = sql_query->rewritten != NULL ?
						 sql_query->rewritten :
						 msg;

		if (client->pending_begin && !sql_query->active) {
			machine_msg_t *rewritten = kiwi_fe_write_query_join(
				NULL, "BEGIN;",
				(char *)machine_msg_data(forward) +
					sizeof(kiwi_header_t),
				NULL);
			if (rewritten == NULL) {
				return OD_EOOM;
			}

			od_wait_event_start(OD_WAIT_QUERY_SERVER_WRITE);
			rc = od_io_write(&server->io, rewritten, timeout_ms);
			od_wait_event_end(OD_WAIT_QUERY_SERVER_WRITE);
			machine_msg_free(rewritten);
		} else {
			od_wait_event_start(OD_WAIT_QUERY_SERVER_WRITE);
			rc = od_io_write(&server->io, forward, timeout_ms);
			od_wait_event_end(OD_WAIT_QUERY_SERVER_WRITE);
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

	if (client->pending_begin && !sql_query->active) {
		/*
		 * need to consume CommandComplete(BEGIN)
		 * and maybe bypass some asynchronious messages
		 */
		status = proxy_until_command_complete(client, server,
						      timeout_ms);
		if (status != OD_OK) {
			return status;
		}
	}
	client->pending_begin = 0;

	uint64_t errors_before = server->error_responses;

	status = od_stream_server_until_rfq(server, timeout_ms,
					    OD_WAIT_STREAM_SERVER_READ);

	if (status == OD_COPY_IN_RECEIVED) {
		/*
		 * server is awaiting CopyData from client - stream it
		 */
		machine_msg_t *add = od_relay_get_copy_additional(relay);
		od_assert(add == NULL);
		status = od_stream_copy_to_server(client, server, add,
						  timeout_ms);
	}

	if (status == OD_OK && server->error_responses == errors_before) {
		/*
		 * process only after successful query completion: a failed
		 * DISCARD ALL or UNLISTEN * still ends with ReadyForQuery
		 */
		process_discard(client, server);
		process_unlisten(client, server);
	}

	if (status == OD_OK) {
		/* checks by itself which statements of the query completed */
		if (sql_query->active) {
			status = od_sql_pstmt_query_finish(client);
		}
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

/*
 * check whether a Parse msg defining the pstmt name exists in xbuf,
 * searching backwards from idx (exclusive)
 *
 * the batch is evaluated in msg order and stops at the first
 * non-friendly msg (see xproto_batch_standby_friendly), so a Parse
 * msg found in the already evaluated prefix is known to be
 * standby friendly
 */
static int xbuf_has_parse(od_relay_t *relay, const char *name, size_t idx)
{
	od_relay_xbuf_t *xbuf = &relay->xbuf;

	for (size_t i = idx; i > 0; --i) {
		od_xbuf_msg_t *m = mm_vector_get(&xbuf->msgs, i - 1);
		if (xbuf_msg_fe_type(m->msg) != KIWI_FE_PARSE) {
			continue;
		}

		const char *pstmt_name = xbuf_msg_name(m->msg, KIWI_FE_PARSE);
		if (pstmt_name != NULL && strcmp(pstmt_name, name) == 0) {
			return 1;
		}
	}

	return 0;
}

/*
 * check whether a Bind msg creating the portal name exists in xbuf,
 * searching backwards from idx (exclusive); the msg ordering
 * invariant is the same as for xbuf_has_parse
 */
static int xbuf_has_bind(od_relay_t *relay, const char *name, size_t idx)
{
	od_relay_xbuf_t *xbuf = &relay->xbuf;

	for (size_t i = idx; i > 0; --i) {
		od_xbuf_msg_t *m = mm_vector_get(&xbuf->msgs, i - 1);
		if (xbuf_msg_fe_type(m->msg) != KIWI_FE_BIND) {
			continue;
		}

		const char *portal_name = xbuf_msg_name(m->msg, KIWI_FE_BIND);
		if (portal_name != NULL && strcmp(portal_name, name) == 0) {
			return 1;
		}
	}

	return 0;
}

static int parse_standby_friendly(od_relay_t *relay, od_xbuf_msg_t *m)
{
	od_client_t *client = relay->client;
	od_instance_t *instance = client->global->instance;

	if (m->pstmt == NULL) {
		od_pstmt_desc_t desc = od_pstmt_desc_from_parse(m->msg);
		if (desc.data == NULL) {
			return 0;
		}

		od_global_pstmt_map_t *global =
			od_instance_get_pstmts_map(instance);
		od_pstmt_t *pstmt = od_pstmt_create_or_get(
			global, desc, od_worker_get_local_linear_alloc(),
			&instance->config.query_parsing);
		if (pstmt == NULL) {
			return 0;
		}

		m->pstmt = pstmt;
	}

	return od_query_ctx_standby_friendly(&m->pstmt->query_ctx);
}

static int pstmt_standby_friendly_by_name(od_relay_t *relay, const char *name,
					  size_t idx)
{
	if (xbuf_has_parse(relay, name, idx)) {
		/*
		 * the pstmt is defined by this very batch - it is not in
		 * the client map yet; the matching Parse msg is in the
		 * already evaluated prefix, so it is known to be
		 * standby friendly
		 */
		return 1;
	}

	od_pstmt_t *pstmt = od_client_get_pstmt(relay->client, name);
	if (pstmt != NULL) {
		return od_query_ctx_standby_friendly(&pstmt->query_ctx);
	}

	return 0;
}

static int bind_standby_friendly(od_relay_t *relay, machine_msg_t *msg,
				 size_t idx)
{
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);

	char *portal, *pstmt_name;
	uint32_t portal_len, pstmt_name_len;
	if (kiwi_be_read_bind_names(data, size, &portal, &portal_len,
				    &pstmt_name, &pstmt_name_len) != 0) {
		return 0;
	}

	(void)portal;
	(void)portal_len;

	return pstmt_standby_friendly_by_name(relay, pstmt_name, idx);
}

static int execute_standby_friendly(od_relay_t *relay, machine_msg_t *msg,
				    size_t idx)
{
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);

	char *portal;
	uint32_t portal_len;
	if (kiwi_be_read_execute(data, size, &portal, &portal_len) != 0) {
		return 0;
	}

	(void)portal_len;

	if (xbuf_has_bind(relay, portal, idx)) {
		/*
		 * the portal is created by this very batch - the matching
		 * Bind msg is in the already evaluated prefix, so it is
		 * known to be standby friendly
		 */
		return 1;
	}

	od_pstmt_t *pstmt = od_client_get_portal(relay->client, portal);
	if (pstmt != NULL) {
		return od_query_ctx_standby_friendly(&pstmt->query_ctx);
	}

	return 0;
}

static inline int describe_standby_friendly(od_relay_t *relay,
					    machine_msg_t *msg)
{
	/*
	* Describe does not execute anything itself - it is neutral
	* for the routing decision
	*/

	(void)relay;
	(void)msg;

	return 1;
}

static int xproto_batch_standby_friendly(od_relay_t *relay)
{
	od_relay_xbuf_t *xbuf = &relay->xbuf;
	size_t count = mm_vector_size(&xbuf->msgs);
	for (size_t i = 0; i < count; ++i) {
		od_xbuf_msg_t *m = mm_vector_get(&xbuf->msgs, i);

		switch (xbuf_msg_fe_type(m->msg)) {
		case KIWI_FE_PARSE:
			if (!parse_standby_friendly(relay, m)) {
				return 0;
			}
			break;
		case KIWI_FE_BIND:
			if (!bind_standby_friendly(relay, m->msg, i)) {
				return 0;
			}
			break;
		case KIWI_FE_EXECUTE:
			if (!execute_standby_friendly(relay, m->msg, i)) {
				return 0;
			}
			break;
		case KIWI_FE_DESCRIBE:
			if (!describe_standby_friendly(relay, m->msg)) {
				return 0;
			}
			break;
		case KIWI_FE_CLOSE:
		case KIWI_FE_FLUSH:
		case KIWI_FE_SYNC:
			/* does not execute anything - neutral */
			break;
		default:
			/*
			 * Query (deferred begin), Copy* and any unexpected
			 * msg - can not tell anything, be conservative
			 */
			return 0;
		}
	}

	return 1;
}

static inline void xproto_seed_query_ctx(od_relay_t *relay)
{
	od_client_t *client = relay->client;

	od_query_ctx_reset(&client->query_ctx);

	if (!od_tsa_auto_route_ro_enabled(client)) {
		return;
	}

	if (!client->rule->pool->reserve_prepared_statement) {
		/*
		 * TODO: without the client pstmt maps Bind/Execute
		 * references can not be resolved, so nothing can be
		 * told about the batch - stay conservative
		 */
		return;
	}

	if (xproto_batch_standby_friendly(relay)) {
		od_query_ctx_set(&client->query_ctx, OD_QUERY_CTX_IS_SELECT);
	}
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
		xproto_seed_query_ctx(relay);
		return OD_ATTACH;
	}

	if (relay_append(relay, msg)) {
		return OD_EOOM;
	}

	od_wait_event_start(OD_WAIT_QUERY_PLANNING);
	status = od_xplan_make_from_xbuf(&relay->xplan, relay);
	od_wait_event_end(OD_WAIT_QUERY_PLANNING);
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

	od_wait_event_start(OD_WAIT_QUERY_SERVER_WRITE);
	int rc = od_io_write(&server->io, msg, timeout_ms);
	od_wait_event_end(OD_WAIT_QUERY_SERVER_WRITE);
	if (rc != 0) {
		return OD_ESERVER_WRITE;
	}

	od_server_sync_request(server, 1);

	od_stat_query_start(&server->stats_state);

	return od_stream_server_until_rfq(server, timeout_ms,
					  OD_WAIT_STREAM_SERVER_READ);
}

od_frontend_status_t od_relay_process_query(od_relay_t *relay,
					    machine_msg_t *msg,
					    uint32_t timeout_ms)
{
	od_frontend_status_t status = process_possible_attach(
		process_query_impl, relay, msg, timeout_ms);

	od_query_ctx_reset(&relay->client->query_ctx);
	if (status == OD_OK) {
		od_sql_pstmt_query_reset(&relay->client->sql_query);
	}

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

	od_query_ctx_reset(&relay->client->query_ctx);

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

	od_query_ctx_reset(&relay->client->query_ctx);

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
