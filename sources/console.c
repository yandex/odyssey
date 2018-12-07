
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>

enum
{
	OD_LKILL_CLIENT,
	OD_LSHOW,
	OD_LSTATS,
	OD_LSERVERS,
	OD_LCLIENTS,
	OD_LLISTS,
	OD_LSET
};

static od_keyword_t
od_console_keywords[] =
{
	od_keyword("kill_client", OD_LKILL_CLIENT),
	od_keyword("show",        OD_LSHOW),
	od_keyword("stats",       OD_LSTATS),
	od_keyword("servers",     OD_LSERVERS),
	od_keyword("clients",     OD_LCLIENTS),
	od_keyword("lists",       OD_LLISTS),
	od_keyword("set",         OD_LSET),
	{ 0, 0, 0 }
};

int
od_console_setup(od_client_t *client)
{
	/* console parameters */
	int rc;
	machine_msg_t *msg;
	msg = kiwi_be_write_parameter_status("server_version", 15, "9.6.0", 6);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_parameter_status("server_encoding", 16, "UTF-8", 6);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_parameter_status("client_encoding", 16, "UTF-8", 6);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_parameter_status("DateStyle", 10, "ISO", 4);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_parameter_status("TimeZone", 9, "GMT", 4);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	/* ready message */
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	rc = machine_flush(client->io, UINT32_MAX);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_stats_add(od_client_t *client,
                          char *database,
                          int   database_len,
                          od_stat_t *total,
                          od_stat_t *avg)
{
	machine_msg_t *msg;
	msg = kiwi_be_write_data_row();
	if (msg == NULL)
		return -1;
	int rc;
	rc = kiwi_be_write_data_row_add(msg, database, database_len);
	if (rc == -1)
		goto error;
	char data[64];
	int  data_len;
	/* total_xact_count */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->count_tx);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* total_query_count */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->count_query);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* total_received */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->recv_client);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* total_sent */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->recv_server);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* total_xact_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->tx_time);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* total_query_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, total->query_time);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* total_wait_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* avg_xact_count */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->count_tx);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* avg_query_count */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->count_query);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* avg_recv */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->recv_client);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* avg_sent */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->recv_server);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* avg_xact_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->tx_time);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* avg_query_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, avg->query_time);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* avg_wait_time */
	data_len = od_snprintf(data, sizeof(data), "%" PRIu64, 0);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;

	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
error:
	machine_msg_free(msg);
	return -1;
}

static int
od_console_show_stats_cb(char *database,
                         int   database_len,
                         od_stat_t *total,
                         od_stat_t *avg, void **argv)
{
	od_client_t *client = argv[0];
	return od_console_show_stats_add(client, database, database_len,
	                                 total, avg);
}

static inline int
od_console_show_stats(od_client_t *client)
{
	od_router_t *router = client->global->router;
	od_cron_t *cron = client->global->cron;

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf("sllllllllllllll",
	                                     "database",
	                                     "total_xact_count",
	                                     "total_query_count",
	                                     "total_received",
	                                     "total_sent",
	                                     "total_xact_time",
	                                     "total_query_time",
	                                     "total_wait_time",
	                                     "avg_xact_count",
	                                     "avg_query_count",
	                                     "avg_recv",
	                                     "avg_sent",
	                                     "avg_xact_time",
	                                     "avg_query_time",
	                                     "avg_wait_time");
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;

	void *argv[] = { client };
	od_route_pool_stat_database(&router->route_pool,
	                            od_console_show_stats_cb,
	                            cron->stat_time_us,
	                            argv);

	/* ready */
	msg = kiwi_be_write_complete("SHOW", 5);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_servers_server_cb(od_server_t *server, void **argv)
{
	od_route_t *route = server->route;

	machine_msg_t *msg;
	msg = kiwi_be_write_data_row();
	if (msg == NULL)
		return -1;
	/* type */
	char data[64];
	int  data_len;
	data_len = od_snprintf(data, sizeof(data), "S");
	int rc;
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* user */
	rc = kiwi_be_write_data_row_add(msg,
	                                route->id.user,
	                                route->id.user_len - 1);
	if (rc == -1)
		goto error;
	/* database */
	rc = kiwi_be_write_data_row_add(msg,
	                                route->id.database,
	                                route->id.database_len - 1);
	if (rc == -1)
		goto error;
	/* state */
	char *state = "";
	if (server->state == OD_SERVER_IDLE)
		state = "idle";
	else
	if (server->state == OD_SERVER_ACTIVE)
		state = "active";
	rc = kiwi_be_write_data_row_add(msg, state, strlen(state));
	if (rc == -1)
		goto error;
	/* addr */
	od_getpeername(server->io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* port */
	od_getpeername(server->io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* local_addr */
	od_getsockname(server->io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* local_port */
	od_getsockname(server->io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* connect_time */
	rc = kiwi_be_write_data_row_add(msg, NULL, -1);
	if (rc == -1)
		goto error;
	/* request_time */
	rc = kiwi_be_write_data_row_add(msg, NULL, -1);
	if (rc == -1)
		goto error;
	/* ptr */
	data_len = od_snprintf(data, sizeof(data), "%s%.*s",
	                       server->id.id_prefix,
	                       (signed)sizeof(server->id.id), server->id.id);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* link */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* remote_pid */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* tls */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;

	od_client_t *origin_client = argv[0];
	rc = machine_write(origin_client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
error:
	machine_msg_free(msg);
	return -1;
}

static inline int
od_console_show_servers_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);

	od_server_pool_foreach(&route->server_pool,
	                       OD_SERVER_ACTIVE,
	                       od_console_show_servers_server_cb,
	                       argv);

	od_server_pool_foreach(&route->server_pool,
	                       OD_SERVER_IDLE,
	                       od_console_show_servers_server_cb,
	                       argv);

	od_route_unlock(route);
	return 0;
}

static inline int
od_console_show_servers(od_client_t *client)
{
	od_router_t *router = client->global->router;

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf("sssssdsdssssds",
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
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;

	void *argv[] = { client };
	od_router_foreach(router, od_console_show_servers_cb, argv);

	/* ready */
	msg = kiwi_be_write_complete("SHOW", 5);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_clients_callback(od_client_t *client, void **argv)
{
	(void)argv;
	machine_msg_t *msg;
	msg = kiwi_be_write_data_row();
	if (msg == NULL)
		return -1;
	char data[64];
	int  data_len;
	/* type */
	data_len = od_snprintf(data, sizeof(data), "C");
	int rc;
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* user */
	rc = kiwi_be_write_data_row_add(msg, kiwi_param_value(client->startup.user),
	                                client->startup.user->value_len - 1);
	if (rc == -1)
		goto error;
	/* database */
	rc = kiwi_be_write_data_row_add(msg, kiwi_param_value(client->startup.database),
	                                client->startup.database->value_len - 1);
	if (rc == -1)
		goto error;
	/* state */
	char *state = "";
	if (client->state == OD_CLIENT_ACTIVE)
		state = "active";
	else
	if (client->state == OD_CLIENT_PENDING)
		state = "pending";
	else
	if (client->state == OD_CLIENT_QUEUE)
		state = "queue";

	rc = kiwi_be_write_data_row_add(msg, state, strlen(state));
	if (rc == -1)
		goto error;
	/* addr */
	od_getpeername(client->io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* port */
	od_getpeername(client->io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* local_addr */
	od_getsockname(client->io, data, sizeof(data), 1, 0);
	data_len = strlen(data);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* local_port */
	od_getsockname(client->io, data, sizeof(data), 0, 1);
	data_len = strlen(data);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* connect_time */
	rc = kiwi_be_write_data_row_add(msg, NULL, -1);
	if (rc == -1)
		goto error;
	/* request_time */
	rc = kiwi_be_write_data_row_add(msg, NULL, -1);
	if (rc == -1)
		goto error;
	/* ptr */
	data_len = od_snprintf(data, sizeof(data), "%s%.*s",
	                       client->id.id_prefix,
	                       (signed)sizeof(client->id.id), client->id.id);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* link */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* remote_pid */
	data_len = od_snprintf(data, sizeof(data), "0");
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* tls */
	data_len = od_snprintf(data, sizeof(data), "%s", "");
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;

	od_client_t *origin_client = argv[0];
	rc = machine_write(origin_client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
error:
	machine_msg_free(msg);
	return -1;
}

static inline int
od_console_show_clients_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);

	od_client_pool_foreach(&route->client_pool,
	                       OD_CLIENT_ACTIVE,
	                       od_console_show_clients_callback,
	                       argv);

	od_client_pool_foreach(&route->client_pool,
	                       OD_CLIENT_PENDING,
	                       od_console_show_clients_callback,
	                       argv);

	od_client_pool_foreach(&route->client_pool,
	                       OD_CLIENT_QUEUE,
	                       od_console_show_clients_callback,
	                       argv);

	od_route_unlock(route);
	return 0;
}

static inline int
od_console_show_clients(od_client_t *client)
{
	od_router_t *router = client->global->router;

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf("sssssdsdssssds",
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
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;

	void *argv[] = { client };
	od_router_foreach(router, od_console_show_clients_cb, argv);

	/* ready */
	msg = kiwi_be_write_complete("SHOW", 5);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_lists_add(od_client_t *client, char *list, int items)
{
	machine_msg_t *msg;
	msg = kiwi_be_write_data_row();
	if (msg == NULL)
		return -1;
	/* list */
	int rc;
	rc = kiwi_be_write_data_row_add(msg, list, strlen(list));
	if (rc == -1) {
		machine_msg_free(msg);
		return -1;
	}
	/* items */
	char data[64];
	int  data_len;
	data_len = od_snprintf(data, sizeof(data), "%d", items);
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1) {
		machine_msg_free(msg);
		return -1;
	}
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show_lists_cb(od_route_t *route, void **argv)
{
	od_route_lock(route);

	int *used_servers = argv[0];
	(*used_servers) += route->server_pool.count_active;
	(*used_servers) += route->server_pool.count_idle;

	od_route_unlock(route);
	return 0;
}

static inline int
od_console_show_lists(od_client_t *client)
{
	od_router_t *router = client->global->router;

	/* Gather router information.

	   router_used_servers can be inconsistent here, since it depends on
	   separate route locks.
	*/
	od_router_lock(router);

	int router_used_servers = 0;
	int router_pools        = router->route_pool.count;
	int router_clients      = router->clients;

	void *argv[] = { &router_used_servers };
	od_route_pool_foreach(&router->route_pool, od_console_show_lists_cb, argv);

	od_router_unlock(router);

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf("sd", "list", "items");
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	/* databases */
	rc = od_console_show_lists_add(client, "databases", 0);
	if (rc == -1)
		return -1;
	/* users */
	rc = od_console_show_lists_add(client, "users", 0);
	if (rc == -1)
		return -1;
	/* pools */
	rc = od_console_show_lists_add(client, "pools", router_pools);
	if (rc == -1)
		return -1;
	/* free_clients */
	rc = od_console_show_lists_add(client, "free_clients", 0);
	if (rc == -1)
		return -1;
	/* used_clients */
	rc = od_console_show_lists_add(client, "used_clients", router_clients);
	if (rc == -1)
		return -1;
	/* login_clients */
	rc = od_console_show_lists_add(client, "login_clients", 0);
	if (rc == -1)
		return -1;
	/* free_servers */
	rc = od_console_show_lists_add(client, "free_servers", 0);
	if (rc == -1)
		return -1;
	/* used_servers */
	rc = od_console_show_lists_add(client, "used_servers", router_used_servers);
	if (rc == -1)
		return -1;
	/* dns_names */
	rc = od_console_show_lists_add(client, "dns_names", 0);
	if (rc == -1)
		return -1;
	/* dns_zones */
	rc = od_console_show_lists_add(client, "dns_zones", 0);
	if (rc == -1)
		return -1;
	/* dns_queries */
	rc = od_console_show_lists_add(client, "dns_queries", 0);
	if (rc == -1)
		return -1;
	/* dns_pending */
	rc = od_console_show_lists_add(client, "dns_pending", 0);
	if (rc == -1)
		return -1;
	/* ready */
	msg = kiwi_be_write_complete("SHOW", 5);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_show(od_client_t *client, od_parser_t *parser)
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
od_console_kill_client(od_client_t *client, od_parser_t *parser)
{
	od_token_t token;
	int rc;
	rc = od_parser_next(parser, &token);
	if (rc != OD_PARSER_KEYWORD)
		return -1;
	od_id_t id;
	if (token.value.string.size != (sizeof(id.id) + 1))
		return -1;
	memcpy(id.id, token.value.string.pointer + 1, sizeof(id.id));

	/* ask client to disconnect */
	od_router_kill(client->global->router, &id);

	/* ready */
	machine_msg_t *msg;
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
}

static inline int
od_console_set(od_client_t *client)
{
	/* reply success */
	machine_msg_t *msg;
	msg = kiwi_be_write_complete("SET", 4);
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	/* ready */
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
}

int
od_console_query(od_client_t *client, machine_msg_t *request)
{
	od_instance_t *instance = client->global->instance;

	uint32_t query_len;
	char *query;
	machine_msg_t *msg;
	int rc;
	rc = kiwi_be_read_query(request, &query, &query_len);
	if (rc == -1) {
		od_error(&instance->logger, "console", client, NULL,
		         "bad console command");
		msg = od_frontend_errorf(client, KIWI_SYNTAX_ERROR, "bad console command");
		if (msg == NULL)
			return -1;
		rc = machine_write(client->io, msg);
		if (rc == -1)
			return -1;
		msg = kiwi_be_write_ready('I');
		if (msg == NULL)
			return -1;
		rc = machine_write(client->io, msg);
		if (rc == -1)
			return -1;
		return 0;
	}

	if (instance->config.log_query)
		od_debug(&instance->logger, "console", client, NULL,
		         "%.*s", query_len, query);

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
		rc = od_console_show(client, &parser);
		if (rc == -1)
			goto bad_query;
		break;
	case OD_LKILL_CLIENT:
		rc = od_console_kill_client(client, &parser);
		if (rc == -1)
			goto bad_query;
		break;
	case OD_LSET:
		rc = od_console_set(client);
		if (rc == -1)
			goto bad_query;
		break;
	default:
		goto bad_query;
	}

	return 0;

bad_query:
	od_error(&instance->logger, "console", client, NULL,
	         "console command error: %.*s", query_len, query);
	msg = od_frontend_errorf(client, KIWI_SYNTAX_ERROR,
	                         "console command error: %.*s",
	                         query_len, query);
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	/* ready */
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	rc = machine_write(client->io, msg);
	if (rc == -1)
		return -1;
	return 0;
}
