
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

typedef struct
{
	od_client_t       *client;
	machine_msg_t     *request;
	machine_channel_t *reply;
	machine_channel_t *on_complete;
} od_msg_console_t;

enum
{
	OD_LKILL_CLIENT,
	OD_LSHOW,
	OD_LSTATS,
	OD_LSERVERS,
	OD_LCLIENTS,
	OD_LLISTS,
	OD_LSET,
	OD_PAUSE,
	OD_RESUME
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
	od_keyword("pause",       OD_PAUSE),
	od_keyword("resume",      OD_RESUME),
	{ 0, 0, 0 }
};

static inline int
od_console_show_stats_add(machine_channel_t *reply,
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

	machine_channel_write(reply, msg);
	return 0;
error:
	machine_msg_free(msg);
	return -1;
}

static int
od_console_show_stats_callback(char *database,
                               int   database_len,
                               od_stat_t *total,
                               od_stat_t *avg, void *arg)
{
	machine_channel_t *reply = arg;
	return od_console_show_stats_add(reply, database, database_len,
	                                 total, avg);
}

static inline int
od_console_show_stats(od_client_t *client, machine_channel_t *reply)
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
	machine_channel_write(reply, msg);

	int rc;
	rc = od_route_pool_stat_database(&router->route_pool,
	                                 od_console_show_stats_callback,
	                                 cron->stat_time_us,
	                                 reply);
	if (rc == -1)
		return -1;

	msg = kiwi_be_write_complete("SHOW", 5);
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);

	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);
	return 0;
}

static inline int
od_console_show_servers_callback(od_server_t *server, void *arg)
{
	od_route_t *route = server->route;

	machine_msg_t *msg;
	msg = kiwi_be_write_data_row();
	if (msg == NULL)
		return -1;

	char data[64];
	int  data_len;

	/* type */
	data_len = od_snprintf(data, sizeof(data), "S");
	int rc;
	rc = kiwi_be_write_data_row_add(msg, data, data_len);
	if (rc == -1)
		goto error;
	/* user */
	rc = kiwi_be_write_data_row_add(msg, route->id.user,
	                                route->id.user_len - 1);
	if (rc == -1)
		goto error;
	/* database */
	rc = kiwi_be_write_data_row_add(msg, route->id.database,
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

	machine_channel_t *reply = arg;
	machine_channel_write(reply, msg);
	return 0;

error:
	machine_msg_free(msg);
	return -1;
}

static inline int
od_console_show_servers(od_client_t *client, machine_channel_t *reply)
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
	machine_channel_write(reply, msg);

	od_route_pool_server_foreach(&router->route_pool,
	                             OD_SERVER_IDLE,
	                             od_console_show_servers_callback,
	                             reply);

	od_route_pool_server_foreach(&router->route_pool,
	                             OD_SERVER_ACTIVE,
	                             od_console_show_servers_callback,
	                             reply);

	msg = kiwi_be_write_complete("SHOW", 5);
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);

	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);
	return 0;
}

static inline int
od_console_show_clients_callback(od_client_t *client, void *arg)
{
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

	machine_channel_t *reply = arg;
	machine_channel_write(reply, msg);
	return 0;
error:
	machine_msg_free(msg);
	return -1;
}

static inline int
od_console_show_clients(od_client_t *client, machine_channel_t *reply)
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
	machine_channel_write(reply, msg);

	od_route_pool_client_foreach(&router->route_pool,
	                             OD_CLIENT_ACTIVE,
	                             od_console_show_clients_callback,
	                             reply);

	od_route_pool_client_foreach(&router->route_pool,
	                             OD_CLIENT_PENDING,
	                             od_console_show_clients_callback,
	                             reply);

	od_route_pool_client_foreach(&router->route_pool,
	                             OD_CLIENT_QUEUE,
	                             od_console_show_clients_callback,
	                             reply);

	msg = kiwi_be_write_complete("SHOW", 5);
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);

	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);
	return 0;
}

static inline int
od_console_show_lists_add(machine_channel_t *reply, char *list, int items)
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
	machine_channel_write(reply, msg);
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
od_console_show_lists(od_client_t *client, machine_channel_t *reply)
{
	od_router_t *router = client->global->router;

	int used_servers = 0;
	od_route_pool_server_foreach(&router->route_pool,
	                             OD_SERVER_IDLE,
	                             od_console_show_lists_callback,
	                             &used_servers);

	od_route_pool_server_foreach(&router->route_pool,
	                             OD_SERVER_ACTIVE,
	                             od_console_show_lists_callback,
	                             &used_servers);

	machine_msg_t *msg;
	msg = kiwi_be_write_row_descriptionf("sd", "list", "items");
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);

	/* databases */
	int rc;
	rc = od_console_show_lists_add(reply, "databases", 0);
	if (rc == -1)
		return -1;
	/* users */
	rc = od_console_show_lists_add(reply, "users", 0);
	if (rc == -1)
		return -1;
	/* pools */
	rc = od_console_show_lists_add(reply, "pools", router->route_pool.count);
	if (rc == -1)
		return -1;
	/* free_clients */
	rc = od_console_show_lists_add(reply, "free_clients", 0);
	if (rc == -1)
		return -1;
	/* used_clients */
	rc = od_console_show_lists_add(reply, "used_clients", router->clients);
	if (rc == -1)
		return -1;
	/* login_clients */
	rc = od_console_show_lists_add(reply, "login_clients", 0);
	if (rc == -1)
		return -1;
	/* free_servers */
	rc = od_console_show_lists_add(reply, "free_servers", 0);
	if (rc == -1)
		return -1;
	/* used_servers */
	rc = od_console_show_lists_add(reply, "used_servers", used_servers);
	if (rc == -1)
		return -1;
	/* dns_names */
	rc = od_console_show_lists_add(reply, "dns_names", 0);
	if (rc == -1)
		return -1;
	/* dns_zones */
	rc = od_console_show_lists_add(reply, "dns_zones", 0);
	if (rc == -1)
		return -1;
	/* dns_queries */
	rc = od_console_show_lists_add(reply, "dns_queries", 0);
	if (rc == -1)
		return -1;
	/* dns_pending */
	rc = od_console_show_lists_add(reply, "dns_pending", 0);
	if (rc == -1)
		return -1;

	msg = kiwi_be_write_complete("SHOW", 5);
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);
	return 0;
}

static inline int
od_console_query_show(od_client_t *client, machine_channel_t *reply,
                      od_parser_t *parser)
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
		return od_console_show_stats(client, reply);
	case OD_LSERVERS:
		return od_console_show_servers(client, reply);
	case OD_LCLIENTS:
		return od_console_show_clients(client, reply);
	case OD_LLISTS:
		return od_console_show_lists(client, reply);
	}
	return -1;
}

static inline int
od_console_query_kill_client_callback(od_client_t *client, void *arg)
{
	od_id_t *id = arg;
	return od_id_mgr_cmp(&client->id, id);
}

static inline od_client_t*
od_console_query_kill_client_match(od_router_t *router, od_id_t *id)
{
	od_client_t *match;
	match = od_route_pool_client_foreach(&router->route_pool,
	                                     OD_CLIENT_ACTIVE,
	                                     od_console_query_kill_client_callback,
	                                     id);
	if (match)
		return match;
	match = od_route_pool_client_foreach(&router->route_pool,
	                                     OD_CLIENT_PENDING,
	                                     od_console_query_kill_client_callback,
	                                     id);
	if (match)
		return match;
	match = od_route_pool_client_foreach(&router->route_pool,
	                                     OD_CLIENT_QUEUE,
	                                     od_console_query_kill_client_callback,
	                                     id);
	return match;
}

static inline int
od_console_query_kill_client(od_router_t *router, machine_channel_t *reply,
                             od_parser_t *parser)
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

	od_client_t *match;
	match = od_console_query_kill_client_match(router, &id);
	if (match) {
		match->ctl.op = OD_CLIENT_OP_KILL;
		od_client_notify(match);
	}

	machine_msg_t *msg;
	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);
	return 0;
}

static inline int
od_console_query_set(machine_channel_t *reply)
{
	machine_msg_t *msg;
	msg = kiwi_be_write_complete("SET", 4);
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);

	msg = kiwi_be_write_complete("SET", 4);
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);

	msg = kiwi_be_write_ready('I');
	if (msg == NULL)
		return -1;
	machine_channel_write(reply, msg);
	return 0;
}

static inline int
od_console_query_set_storage_state(od_router_t *router, machine_channel_t *reply,
					   od_parser_t *parser, od_client_t *client, od_storage_state_t state)
{
    machine_msg_t *msg;
    od_instance_t *instance = router->global->instance;
    od_config_t *config = &instance->config;

    char *storage_name = "";
    od_token_t token;
    int rc = od_parser_next(parser, &token);

    char *state_name = state == OD_STORAGE_PAUSE ? "PAUSED" : "RESUME";

    if (rc == OD_PARSER_KEYWORD)
    {
        storage_name = token.value.string.pointer;
        od_debug(&instance->logger, "console", client, NULL,
                "Making storage %s %s...", token.value.string.pointer, state_name);
    }
    else
    {
        od_debug(&instance->logger, "console", client, NULL,
                "Making all storage %s...", state_name);

    }

    bool pause_all = strcmp(storage_name, "") == 0;

    od_list_t *i;
    od_list_foreach(&config->routes, i) {
        od_config_route_t *config_route;
        config_route = od_container_of(i, od_config_route_t, link);
        if(config_route->storage->storage_type == OD_STORAGE_TYPE_REMOTE)
        {
            if (pause_all || strcmp(config_route->storage->name, storage_name))
            {
                config_route->storage->state = state;

                if (state == OD_STORAGE_PAUSE)
                {
                    od_list_t *j;
                    od_list_foreach(&router->route_pool.list, j) {
                        od_route_t *route;
                        route = od_container_of(j, od_route_t, link);
                        if (route->config->storage->name == config_route->storage->name)
                        {
                            for(;;)
                            {
                                if(!route->server_pool.count_idle && !route->server_pool.count_active)
                                    break;
                                machine_sleep(1000);
                            }
                        }
                    }
                }
                if (!pause_all)
                    break;
            }
        }
    }

    msg = kiwi_be_write_complete(state_name, 7);
    if (msg == NULL)
        return -1;
    machine_channel_write(reply, msg);

    msg = kiwi_be_write_ready('I');
    if (msg == NULL)
        return -1;
    machine_channel_write(reply, msg);
    return 0;
}

static inline void
od_console_query(od_console_t *console, od_msg_console_t *msg_console)
{
	od_instance_t *instance = console->global->instance;
	od_router_t *router = console->global->router;
	od_client_t *client = msg_console->client;

	uint32_t query_len;
	char *query;
	machine_msg_t *msg;
	int rc;
	rc = kiwi_be_read_query(msg_console->request, &query, &query_len);
	if (rc == -1) {
		od_error(&instance->logger, "console", client, NULL,
		         "bad console command");
		msg = od_frontend_errorf(client, KIWI_SYNTAX_ERROR, "bad console command");
		if (msg)
			machine_channel_write(msg_console->reply, msg);
		msg = kiwi_be_write_ready('I');
		if (msg)
			machine_channel_write(msg_console->reply, msg);
		return;
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
	case OD_LKILL_CLIENT:
		rc = od_console_query_kill_client(router, msg_console->reply, &parser);
		if (rc == -1)
			goto bad_query;
		break;
	case OD_LSHOW:
		rc = od_console_query_show(client, msg_console->reply, &parser);
		if (rc == -1)
			goto bad_query;
		break;
	case OD_LSET:
		rc = od_console_query_set(msg_console->reply);
		if (rc == -1)
			goto bad_query;
		break;
	case OD_PAUSE:
		rc = od_console_query_set_storage_state(router, msg_console->reply, &parser, client, OD_STORAGE_PAUSE);
		if (rc == -1)
			goto bad_query;
		break;
	case OD_RESUME:
		rc = od_console_query_set_storage_state(router, msg_console->reply, &parser, client, OD_STORAGE_ACTIVE);
		if (rc == -1)
			goto bad_query;
		break;
	default:
		goto bad_query;
	}

	return;

bad_query:
	od_error(&instance->logger, "console", client, NULL,
	         "console command error: %.*s", query_len, query);
	msg = od_frontend_errorf(client, KIWI_SYNTAX_ERROR, "console command error: %.*s",
	                         query_len, query);
	if (msg)
		machine_channel_write(msg_console->reply, msg);
	msg = kiwi_be_write_ready('I');
	if (msg)
		machine_channel_write(msg_console->reply, msg);
}

static void
od_console(void *arg)
{
	od_console_t *console = arg;
	for (;;)
	{
		machine_msg_t *msg;
		msg = machine_channel_read(console->channel, UINT32_MAX);
		if (msg == NULL)
			break;
		od_msg_t msg_type;
		msg_type = machine_msg_get_type(msg);
		switch (msg_type) {
		case OD_MCONSOLE_REQUEST:
		{
			od_msg_console_t *msg_console;
			msg_console = machine_msg_get_data(msg);
			od_console_query(console, msg_console);
			machine_channel_write(msg_console->on_complete, msg);
			break;
		}
		default:
			assert(0);
			break;
		}
	}
}

void
od_console_init(od_console_t *console, od_global_t *global)
{
	console->global  = global;
	console->channel = NULL;
}

int
od_console_start(od_console_t *console)
{
	od_instance_t *instance = console->global->instance;

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

static inline int
od_console_do(od_client_t *client, machine_channel_t *channel,
              od_msg_t type,
              machine_msg_t *request)
{
	od_instance_t *instance = client->global->instance;
	od_console_t *console = client->global->console;

	/* send request to console */
	machine_msg_t *msg;
	msg = machine_msg_create(sizeof(od_msg_console_t));
	if (msg == NULL)
		return -1;
	machine_msg_set_type(msg, type);

	od_msg_console_t *msg_console;
	msg_console = machine_msg_get_data(msg);
	msg_console->client  = client;
	msg_console->request = request;
	msg_console->reply   = channel;

	/* create response channel */
	machine_channel_t *on_complete;
	on_complete = machine_channel_create(instance->is_shared);
	if (on_complete == NULL) {
		machine_msg_free(msg);
		return -1;
	}
	msg_console->on_complete = on_complete;
	machine_channel_write(console->channel, msg);

	/* wait for reply */
	msg = machine_channel_read(on_complete, UINT32_MAX);
	machine_channel_free(on_complete);
	if (msg == NULL) {
		abort();
		return -1;
	}
	machine_msg_free(msg);
	return 0;
}

int
od_console_request(od_client_t *client, machine_channel_t *channel,
                   machine_msg_t *request)
{
	return od_console_do(client, channel, OD_MCONSOLE_REQUEST, request);
}
