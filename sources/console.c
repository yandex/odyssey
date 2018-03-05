
/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/scheme.h"
#include "sources/scheme_mgr.h"
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/system.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router_cancel.h"
#include "sources/router.h"
#include "sources/pooler.h"
#include "sources/worker.h"
#include "sources/frontend.h"
#include "sources/backend.h"
#include "sources/console.h"
#include "sources/parser.h"

typedef struct
{
	od_consolestatus_t status;
	od_client_t *client;
	char *request;
	int request_len;
	machine_channel_t *response;
} od_msgconsole_t;

enum
{
	OD_LSHOW,
	OD_LSTATS,
	OD_LSERVERS,
	OD_LCLIENTS,
	OD_LLISTS,
	OD_LSET
};

static od_keyword_t od_console_keywords[] =
{
	od_keyword("show",    OD_LSHOW),
	od_keyword("stats",   OD_LSTATS),
	od_keyword("servers", OD_LSERVERS),
	od_keyword("clients", OD_LCLIENTS),
	od_keyword("lists",   OD_LLISTS),
	od_keyword("set",     OD_LSET),
	{ 0, 0, 0 }
};

static inline int
od_console_show_stats_add(shapito_stream_t *stream,
                          char *database,
                          int   database_len,
                          od_serverstat_t *total,
                          od_serverstat_t *avg)
{
	int offset;
	offset = shapito_be_write_data_row(stream);
	int rc;
	rc = shapito_be_write_data_row_add(stream, offset, database, database_len);
	if (rc == -1)
		return -1;
	char data[64];
	int  data_len;
	/* total_requests */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->count_request);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_received */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->recv_client);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_sent */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->recv_server);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* total_query_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->query_time);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_req */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->count_request);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_recv */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->recv_client);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_sent */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->recv_server);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* avg_query */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->query_time);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	return 0;
}

static int
od_console_show_stats_callback(char *database,
                               int   database_len,
                               od_serverstat_t *total,
                               od_serverstat_t *avg, void *arg)
{
	od_client_t *client = arg;
	return od_console_show_stats_add(client->stream,
	                                 database, database_len,
	                                 total, avg);
}

static inline int
od_console_show_stats(od_client_t *client)
{
	od_router_t *router = client->system->router;
	shapito_stream_t *stream = client->stream;
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_be_write_row_descriptionf(stream, "sllllllll",
	                                       "database",
	                                       "total_requests",
	                                       "total_received",
	                                       "total_sent",
	                                       "total_query_time",
	                                       "avg_req",
	                                       "avg_recv",
	                                       "avg_sent",
	                                       "avg_query");
	if (rc == -1)
		return -1;
	rc = od_routepool_stats(&router->route_pool,
	                        od_console_show_stats_callback,
	                        client);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_complete(stream, "SHOW", 5);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_ready(stream, 'I');
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_servers_callback(od_server_t *server, void *arg)
{
	shapito_stream_t *stream = arg;
	od_route_t *route = server->route;

	int offset;
	offset = shapito_be_write_data_row(stream);

	char data[64];
	int  data_len;

	/* type */
	data_len = od_snprintf(data, sizeof(data), "S");
	int rc;
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* user */
	rc = shapito_be_write_data_row_add(stream, offset,
	                                   route->id.user,
	                                   route->id.user_len - 1);
	if (rc == -1)
		return -1;
	/* database */
	rc = shapito_be_write_data_row_add(stream, offset,
	                                   route->id.database,
	                                   route->id.database_len - 1);
	if (rc == -1)
		return -1;
	/* state */
	char *state = "";
	if (server->state == OD_SIDLE)
		state = "idle";
	else
	if (server->state == OD_SACTIVE)
		state = "active";
	rc = shapito_be_write_data_row_add(stream, offset,
	                                   state,
	                                   strlen(state));
	if (rc == -1)
		return -1;
	/* addr */
	od_getpeername(server->io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* port */
	od_getpeername(server->io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* local_addr */
	od_getsockname(server->io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* local_port */
	od_getsockname(server->io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* connect_time */
	rc = shapito_be_write_data_row_add(stream, offset, NULL, -1);
	if (rc == -1)
		return -1;
	/* request_time */
	rc = shapito_be_write_data_row_add(stream, offset, NULL, -1);
	if (rc == -1)
		return -1;
	/* ptr */
	data_len = od_snprintf(data, sizeof(data), "%s%.*s",
	                       server->id.id_prefix,
	                       (signed)sizeof(server->id.id), server->id.id);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* link */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* remote_pid */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* tls */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_servers(od_client_t *client)
{
	od_router_t *router = client->system->router;
	shapito_stream_t *stream = client->stream;
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_be_write_row_descriptionf(stream, "sssssdsdssssds",
	                                       "type",
	                                       "user",
	                                       "database",
	                                       "state",
	                                       "addr",
	                                       "port",
	                                       "local_addr",
	                                       "local_port",
	                                       "connect_time",
	                                       "request_time",
	                                       "ptr",
	                                       "link",
	                                       "remote_pid",
	                                       "tls");
	if (rc == -1)
		return -1;

	od_routepool_server_foreach(&router->route_pool, OD_SIDLE,
	                            od_console_show_servers_callback,
	                            stream);

	od_routepool_server_foreach(&router->route_pool, OD_SACTIVE,
	                            od_console_show_servers_callback,
	                            stream);

	rc = shapito_be_write_complete(stream, "SHOW", 5);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_ready(stream, 'I');
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_clients_callback(od_client_t *client, void *arg)
{
	shapito_stream_t *stream = arg;
	(void)client;
	(void)stream;
	(void)arg;

	int offset;
	offset = shapito_be_write_data_row(stream);

	char data[64];
	int  data_len;

	/* type */
	data_len = od_snprintf(data, sizeof(data), "C");
	int rc;
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* user */
	rc = shapito_be_write_data_row_add(stream, offset,
	                                   shapito_parameter_value(client->startup.user),
	                                   client->startup.user->value_len - 1);
	if (rc == -1)
		return -1;

	/* database */
	rc = shapito_be_write_data_row_add(stream, offset,
	                                   shapito_parameter_value(client->startup.database),
	                                   client->startup.database->value_len - 1);
	if (rc == -1)
		return -1;
	/* state */
	char *state = "";
	if (client->state == OD_CACTIVE)
		state = "active";
	else
	if (client->state == OD_CPENDING)
		state = "pending";
	else
	if (client->state == OD_CQUEUE)
		state = "queue";
	rc = shapito_be_write_data_row_add(stream, offset,
	                                   state,
	                                   strlen(state));
	if (rc == -1)
		return -1;
	/* addr */
	od_getpeername(client->io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* port */
	od_getpeername(client->io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* local_addr */
	od_getsockname(client->io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* local_port */
	od_getsockname(client->io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* connect_time */
	rc = shapito_be_write_data_row_add(stream, offset, NULL, -1);
	if (rc == -1)
		return -1;
	/* request_time */
	rc = shapito_be_write_data_row_add(stream, offset, NULL, -1);
	if (rc == -1)
		return -1;
	/* ptr */
	data_len = od_snprintf(data, sizeof(data), "%s%.*s",
	                       client->id.id_prefix,
	                       (signed)sizeof(client->id.id), client->id.id);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* link */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* remote_pid */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	/* tls */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_clients(od_client_t *client)
{
	od_router_t *router = client->system->router;
	shapito_stream_t *stream = client->stream;
	shapito_stream_reset(stream);
	int rc;
	rc = shapito_be_write_row_descriptionf(stream, "sssssdsdssssds",
	                                       "type",
	                                       "user",
	                                       "database",
	                                       "state",
	                                       "addr",
	                                       "port",
	                                       "local_addr",
	                                       "local_port",
	                                       "connect_time",
	                                       "request_time",
	                                       "ptr",
	                                       "link",
	                                       "remote_pid",
	                                       "tls");
	if (rc == -1)
		return -1;

	od_routepool_client_foreach(&router->route_pool, OD_CACTIVE,
	                            od_console_show_clients_callback,
	                            stream);

	od_routepool_client_foreach(&router->route_pool, OD_CPENDING,
	                            od_console_show_clients_callback,
	                            stream);

	od_routepool_client_foreach(&router->route_pool, OD_CQUEUE,
	                            od_console_show_clients_callback,
	                            stream);

	rc = shapito_be_write_complete(stream, "SHOW", 5);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_ready(stream, 'I');
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_lists_add(shapito_stream_t *stream, char *list, int items)
{
	int offset;
	offset = shapito_be_write_data_row(stream);
	/* list */
	int rc;
	rc = shapito_be_write_data_row_add(stream, offset, list, strlen(list));
	if (rc == -1)
		return -1;
	/* items */
	char data[64];
	int  data_len;
	data_len = od_snprintf(data, sizeof(data), "%d", items);
	rc = shapito_be_write_data_row_add(stream, offset, data, data_len);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_lists_callback(od_server_t *server, void *arg)
{
	(void)server;
	int *used_servers = arg;
	(*used_servers)++;
	return 0;
}

static inline int
od_console_show_lists(od_client_t *client)
{
	od_router_t *router = client->system->router;
	shapito_stream_t *stream = client->stream;
	shapito_stream_reset(stream);

	int used_servers = 0;
	od_routepool_server_foreach(&router->route_pool, OD_SIDLE,
	                            od_console_show_lists_callback,
	                            &used_servers);
	od_routepool_server_foreach(&router->route_pool, OD_SACTIVE,
	                            od_console_show_lists_callback,
	                            &used_servers);

	int rc;
	rc = shapito_be_write_row_descriptionf(stream, "sd",
	                                       "list",
	                                       "items");
	if (rc == -1)
		return -1;
	/* databases */
	rc = od_console_show_lists_add(stream, "databases", 0);
	if (rc == -1)
		return -1;
	/* users */
	rc = od_console_show_lists_add(stream, "users", 0);
	if (rc == -1)
		return -1;
	/* pools */
	rc = od_console_show_lists_add(stream, "pools", router->route_pool.count);
	if (rc == -1)
		return -1;
	/* free_clients */
	rc = od_console_show_lists_add(stream, "free_clients", 0);
	if (rc == -1)
		return -1;
	/* used_clients */
	rc = od_console_show_lists_add(stream, "used_clients", router->clients);
	if (rc == -1)
		return -1;
	/* login_clients */
	rc = od_console_show_lists_add(stream, "login_clients", 0);
	if (rc == -1)
		return -1;
	/* free_servers */
	rc = od_console_show_lists_add(stream, "free_servers", 0);
	if (rc == -1)
		return -1;
	/* used_servers */
	rc = od_console_show_lists_add(stream, "used_servers", used_servers);
	if (rc == -1)
		return -1;
	/* dns_names */
	rc = od_console_show_lists_add(stream, "dns_names", 0);
	if (rc == -1)
		return -1;
	/* dns_zones */
	rc = od_console_show_lists_add(stream, "dns_zones", 0);
	if (rc == -1)
		return -1;
	/* dns_queries */
	rc = od_console_show_lists_add(stream, "dns_queries", 0);
	if (rc == -1)
		return -1;
	/* dns_pending */
	rc = od_console_show_lists_add(stream, "dns_pending", 0);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_complete(stream, "SHOW", 5);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_ready(stream, 'I');
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_query_show(od_client_t *client, od_parser_t *parser)
{
	od_token_t token;
	int rc;
	rc = od_parser_next(parser, &token);
	switch (rc) {
	case OD_PARSER_KEYWORD:
		break;
	case OD_PARSER_EOF:
	default:
		return -1;
	}
	od_keyword_t *keyword;
	keyword = od_keyword_match(od_console_keywords, &token);
	if (keyword == NULL)
		return -1;
	switch (keyword->id) {
	case OD_LSTATS:
		return od_console_show_stats(client);
	case OD_LSERVERS:
		return od_console_show_servers(client);
	case OD_LCLIENTS:
		return od_console_show_clients(client);
	case OD_LLISTS:
		return od_console_show_lists(client);
	}
	return -1;
}

static inline int
od_console_query_set(od_client_t *client, od_parser_t *parser)
{
	shapito_stream_t *stream = client->stream;
	shapito_stream_reset(stream);
	(void)parser;
	int rc;
	rc = shapito_be_write_complete(stream, "SET", 4);
	if (rc == -1)
		return -1;
	rc = shapito_be_write_ready(stream, 'I');
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_query(od_console_t *console, od_msgconsole_t *msg_console)
{
	od_instance_t *instance = console->system->instance;
	od_client_t *client = msg_console->client;
	int rc;

	uint32_t query_len;
	char *query;
	rc = shapito_be_read_query(&query, &query_len, msg_console->request,
	                           msg_console->request_len);
	if (rc == -1)
		goto bad_command;

	if (instance->scheme.log_query) {
		od_debug(&instance->logger, "console", client, NULL,
		         "%.*s", query_len, query);
	}

	od_parser_t parser;
	od_parser_init(&parser, query, query_len);

	od_token_t token;
	rc = od_parser_next(&parser, &token);
	switch (rc) {
	case OD_PARSER_KEYWORD:
		break;
	case OD_PARSER_EOF:
	default:
		goto bad_query;
	}
	od_keyword_t *keyword;
	keyword = od_keyword_match(od_console_keywords, &token);
	if (keyword == NULL)
		goto bad_query;
	switch (keyword->id) {
	case OD_LSHOW:
		rc = od_console_query_show(client, &parser);
		if (rc == -1)
			goto bad_query;
		break;
	case OD_LSET:
		rc = od_console_query_set(client, &parser);
		if (rc == -1)
			goto bad_query;
		break;
	default:
		goto bad_query;
	}

	return 0;

bad_query:
	od_error(&instance->logger, "console", client, NULL,
	         "bad console command: %.*s", query_len, query);
	shapito_stream_reset(client->stream);
	od_frontend_errorf(client, SHAPITO_SYNTAX_ERROR, "bad console command: %.*s",
	                   query_len, query);
	shapito_be_write_ready(client->stream, 'I');
	return -1;

bad_command:
	od_error(&instance->logger, "console", client, NULL,
	         "bad console command");
	shapito_stream_reset(client->stream);
	od_frontend_errorf(client, SHAPITO_SYNTAX_ERROR, "bad console command");
	shapito_be_write_ready(client->stream, 'I');
	return -1;
}

static void
od_console(void *arg)
{
	od_console_t *console = arg;
	od_instance_t *instance = console->system->instance;
	(void)instance;

	for (;;) {
		machine_msg_t *msg;
		msg = machine_channel_read(console->channel, UINT32_MAX);
		if (msg == NULL)
			break;
		od_msg_t msg_type;
		msg_type = machine_msg_get_type(msg);
		switch (msg_type) {
		case OD_MCONSOLE_REQUEST:
		{
			od_msgconsole_t *msg_console;
			msg_console = machine_msg_get_data(msg);
			int rc;
			rc = od_console_query(console, msg_console);
			if (rc == -1) {
				msg_console->status = OD_CERROR;
			} else {
				msg_console->status = OD_COK;
			}
			machine_channel_write(msg_console->response, msg);
			break;
		}
		default:
			assert(0);
			break;
		}
	}
}

void od_console_init(od_console_t *console, od_system_t *system)
{
	console->system  = system;
	console->channel = NULL;
}

int od_console_start(od_console_t *console)
{
	od_instance_t *instance = console->system->instance;

	console->channel = machine_channel_create(instance->is_shared);
	if (console->channel == NULL) {
		od_error(&instance->logger, "console", NULL, NULL,
		         "failed to create channel");
		return -1;
	}
	int64_t coroutine_id;
	coroutine_id = machine_coroutine_create(od_console, console);
	if (coroutine_id == -1) {
		od_error(&instance->logger, "console", NULL, NULL,
		         "failed to start console coroutine");
		return -1;
	}
	return 0;
}

static od_consolestatus_t
od_console_do(od_client_t *client, od_msg_t msg_type, char *request, int request_len,
              int wait_for_response)
{
	od_console_t *console = client->system->console;
	od_instance_t *instance = console->system->instance;

	/* send request to console */
	machine_msg_t *msg;
	msg = machine_msg_create(msg_type, sizeof(od_msgconsole_t));
	if (msg == NULL)
		return OD_CERROR;
	od_msgconsole_t *msg_console;
	msg_console = machine_msg_get_data(msg);
	msg_console->status = OD_CERROR;
	msg_console->client = client;
	msg_console->request = request;
	msg_console->request_len = request_len;
	msg_console->response = NULL;

	/* create response channel */
	machine_channel_t *response;
	if (wait_for_response) {
		response = machine_channel_create(instance->is_shared);
		if (response == NULL) {
			machine_msg_free(msg);
			return OD_CERROR;
		}
		msg_console->response = response;
	}
	machine_channel_write(console->channel, msg);

	if (! wait_for_response)
		return OD_COK;

	/* wait for reply */
	msg = machine_channel_read(response, UINT32_MAX);
	if (msg == NULL) {
		/* todo:  */
		abort();
		machine_channel_free(response);
		return OD_CERROR;
	}
	msg_console = machine_msg_get_data(msg);
	od_consolestatus_t status;
	status = msg_console->status;
	machine_channel_free(response);
	machine_msg_free(msg);
	return status;
}

od_consolestatus_t
od_console_request(od_client_t *client, char *request, int request_len)
{
	return od_console_do(client, OD_MCONSOLE_REQUEST, request, request_len, 1);
}
