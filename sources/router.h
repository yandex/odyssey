#ifndef ODYSSEY_ROUTER_H
#define ODYSSEY_ROUTER_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_router od_router_t;

typedef enum
{
	OD_ROUTER_OK,
	OD_ROUTER_ERROR,
	OD_ROUTER_ERROR_NOT_FOUND,
	OD_ROUTER_ERROR_LIMIT,
	OD_ROUTER_ERROR_LIMIT_ROUTE,
	OD_ROUTER_ERROR_TIMEDOUT
} od_router_status_t;

struct od_router
{
	mcxt_context_t	mcxt;
	pthread_mutex_t lock;
	od_rules_t      rules;
	od_route_pool_t route_pool;
	int             clients;
};

static inline void
od_router_lock(od_router_t *router)
{
	pthread_mutex_lock(&router->lock);
}

static inline void
od_router_unlock(od_router_t *router)
{
	pthread_mutex_unlock(&router->lock);
}

void od_router_init(mcxt_context_t, od_router_t*);
void od_router_free(od_router_t*);
int  od_router_reconfigure(od_router_t*, od_rules_t*);
int  od_router_expire(od_router_t*, od_list_t*);
void od_router_gc(od_router_t*);
void od_router_stat(od_router_t*, uint64_t, int, od_route_pool_stat_cb_t, void**);
int  od_router_foreach(od_router_t*, od_route_pool_cb_t, void**);

od_router_status_t
od_router_route(od_router_t*, od_config_t*, od_client_t*);

void
od_router_unroute(od_router_t*, od_client_t*);

od_router_status_t
od_router_attach(od_router_t*, od_config_t*, od_client_t*);

void
od_router_detach(od_router_t*, od_config_t*, od_client_t*);

void
od_router_close(od_router_t*, od_client_t*);

od_router_status_t
od_router_cancel(od_router_t*, kiwi_key_t*, od_router_cancel_t*);

void
od_router_kill(od_router_t*, od_id_t*);

#endif /* ODYSSEY_ROUTER_H */
