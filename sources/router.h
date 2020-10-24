#ifndef ODYSSEY_ROUTER_H
#define ODYSSEY_ROUTER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef struct od_router od_router_t;

struct od_router
{
	pthread_mutex_t lock;
	od_rules_t rules;
	od_list_t servers;
	od_route_pool_t route_pool;
	od_atomic_u32_t clients;
	od_atomic_u32_t clients_routing;
	od_atomic_u32_t servers_routing;

	od_error_logger_t *router_err_logger;
};

inline void
od_router_lock(od_router_t *router)
{
	pthread_mutex_lock(&router->lock);
}

inline void
od_router_unlock(od_router_t *router)
{
	pthread_mutex_unlock(&router->lock);
}

void
od_router_init(od_router_t *);
void
od_router_free(od_router_t *);
int
od_router_reconfigure(od_router_t *, od_rules_t *);
int
od_router_expire(od_router_t *, od_list_t *);
void
od_router_gc(od_router_t *);
void
od_router_stat(od_router_t *, uint64_t, int, od_route_pool_stat_cb_t, void **);
extern int
od_router_foreach(od_router_t *, od_route_pool_cb_t, void **);

od_router_status_t
od_router_route(od_router_t *router, od_config_t *config, od_client_t *client);

void
od_router_unroute(od_router_t *, od_client_t *);

od_router_status_t
od_router_attach(od_router_t *, od_config_t *, od_client_t *, bool);

void
od_router_detach(od_router_t *, od_config_t *, od_client_t *);

void
od_router_close(od_router_t *, od_client_t *);

od_router_status_t
od_router_cancel(od_router_t *, kiwi_key_t *, od_router_cancel_t *);

void
od_router_kill(od_router_t *, od_id_t *);

static inline int
od_route_pool_stat_err_router(od_router_t *router,
                              od_route_pool_stat_route_error_cb_t callback,
                              void **argv)
{
	return callback(router->router_err_logger, argv);
}

#endif /* ODYSSEY_ROUTER_H */
