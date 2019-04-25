#ifndef ODYSSEY_ROUTE_H
#define ODYSSEY_ROUTE_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_route od_route_t;

struct od_route
{
	mcxt_context_t		mcxt;
	od_rule_t          *rule;
	od_route_id_t       id;
	od_stat_t           stats;
	od_stat_t           stats_prev;
	int                 stats_mark;
	od_server_pool_t    server_pool;
	od_client_pool_t    client_pool;
	kiwi_params_lock_t  params;
	machine_channel_t  *wait_bus;
	pthread_mutex_t     lock;
	od_list_t           link;
};

static inline void
od_route_init(od_route_t *route)
{
	route->rule = NULL;
	od_route_id_init(&route->id);
	od_server_pool_init(&route->server_pool);
	od_client_pool_init(&route->client_pool);
	route->stats_mark = 0;
	od_stat_init(&route->stats);
	od_stat_init(&route->stats_prev);
	kiwi_params_lock_init(&route->params);
	od_list_init(&route->link);
	route->wait_bus = NULL;
	pthread_mutex_init(&route->lock, NULL);
}

static inline void
od_route_free(od_route_t *route)
{
	od_server_pool_free(&route->server_pool);
	kiwi_params_lock_free(&route->params);
	if (route->wait_bus)
		machine_channel_free(route->wait_bus);
	pthread_mutex_destroy(&route->lock);
	mcxt_delete(route->mcxt);
}

static inline od_route_t*
od_route_allocate(mcxt_context_t mcxt, int is_shared)
{
	mcxt_context_t route_mcxt = mcxt_new(mcxt);
	od_route_t *route = mcxt_alloc_mem(route_mcxt, sizeof(*route), true);
	if (route == NULL)
		return NULL;

	od_route_init(route);
	route->wait_bus = machine_channel_create(is_shared);
	route->mcxt = route_mcxt;

	if (route->wait_bus == NULL) {
		od_route_free(route);
		return NULL;
	}
	return route;
}

static inline void
od_route_lock(od_route_t *route)
{
	pthread_mutex_lock(&route->lock);
}

static inline void
od_route_unlock(od_route_t *route)
{
	pthread_mutex_unlock(&route->lock);
}

static inline int
od_route_is_dynamic(od_route_t *route)
{
	return route->rule->db_is_default || route->rule->user_is_default;
}

static inline int
od_route_match_compare_client_cb(od_client_t *client, void **argv)
{
	return od_id_cmp(&client->id, argv[0]);
}

static inline od_client_t*
od_route_match_client(od_route_t *route, od_id_t *id)
{
	void *argv[] = { id };
	od_client_t *match;
	match = od_client_pool_foreach(&route->client_pool, OD_CLIENT_ACTIVE,
	                               od_route_match_compare_client_cb,
	                               argv);
	if (match)
		return match;
	match = od_client_pool_foreach(&route->client_pool, OD_CLIENT_QUEUE,
	                               od_route_match_compare_client_cb,
	                               argv);
	if (match)
		return match;
	match = od_client_pool_foreach(&route->client_pool, OD_CLIENT_PENDING,
	                               od_route_match_compare_client_cb,
	                               argv);
	if (match)
		return match;

	return NULL;
}

static inline void
od_route_kill_client(od_route_t *route, od_id_t *id)
{
	od_client_t *client;
	client = od_route_match_client(route, id);
	if (client)
		od_client_kill(client);
}

static inline int
od_route_kill_cb(od_client_t *client, void **argv)
{
	(void)argv;
	od_client_kill(client);
	return 0;
}

static inline void
od_route_kill_client_pool(od_route_t *route)
{
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_ACTIVE,
	                       od_route_kill_cb, NULL);
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_PENDING,
	                       od_route_kill_cb, NULL);
	od_client_pool_foreach(&route->client_pool, OD_CLIENT_QUEUE,
	                       od_route_kill_cb, NULL);
}

static inline int
od_route_wait(od_route_t *route, uint32_t time_ms)
{
	machine_msg_t *msg;
	msg = machine_channel_read(route->wait_bus, time_ms);
	if (msg) {
		machine_msg_free(msg);
		return 0;
	}
	return -1;
}

static inline int
od_route_signal(od_route_t *route)
{
	machine_msg_t *msg;
	msg = machine_msg_create(0);
	if (msg == NULL)
		return -1;
	machine_channel_write(route->wait_bus, msg);
	return 0;
}

#endif /* ODYSSEY_ROUTE_H */
