#ifndef ODYSSEY_ROUTE_POOL_H
#define ODYSSEY_ROUTE_POOL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_route_pool od_route_pool_t;

typedef int (*od_route_pool_cb_t)(od_route_t*, void*);

typedef int (*od_route_pool_stat_cb_t)
             (od_route_t *route,
              od_stat_t *current,
              od_stat_t *avg, void *arg);

typedef int (*od_route_pool_stat_database_cb_t)
             (char *database,
              int   database_len,
              od_stat_t *total,
              od_stat_t *avg, void *arg);

struct od_route_pool
{
	od_list_t list;
	int count;
};

void od_route_pool_init(od_route_pool_t*);
void od_route_pool_free(od_route_pool_t*);
void od_route_pool_gc(od_route_pool_t*);

od_route_t*
od_route_pool_new(od_route_pool_t*, od_config_route_t*,
                  od_route_id_t*);

od_route_t*
od_route_pool_match(od_route_pool_t*, od_route_id_t*, od_config_route_t*);

od_server_t*
od_route_pool_next(od_route_pool_t*, od_server_state_t);

int od_route_pool_foreach(od_route_pool_t*, od_route_pool_cb_t, void*);

od_server_t*
od_route_pool_server_foreach(od_route_pool_t*, od_server_state_t,
                             od_server_pool_cb_t, void*);

od_client_t*
od_route_pool_client_foreach(od_route_pool_t*, od_client_state_t,
                            od_client_pool_cb_t, void*);

int od_route_pool_stat_database(od_route_pool_t *pool,
                                od_route_pool_stat_database_cb_t,
                                uint64_t,
                                void*);

int od_route_pool_stat(od_route_pool_t *pool,
                       od_route_pool_stat_cb_t,
                       uint64_t,
                       void*);

#endif /* ODYSSEY_ROUTE_POOL_H */
