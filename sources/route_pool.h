#ifndef OD_ROUTE_POOL_H
#define OD_ROUTE_POOL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_routepool od_routepool_t;

typedef int (*od_routepool_stat_cb_t)
             (od_route_t *route,
              od_stat_t *current,
              od_stat_t *avg, void *arg);

typedef int (*od_routepool_stat_database_cb_t)
             (char *database,
              int   database_len,
              od_stat_t *total,
              od_stat_t *avg, void *arg);

struct od_routepool
{
	od_list_t list;
	int count;
};

void od_routepool_init(od_routepool_t*);
void od_routepool_free(od_routepool_t*);
void od_routepool_gc(od_routepool_t*);

od_route_t*
od_routepool_new(od_routepool_t*, od_configroute_t*,
                 od_routeid_t*);

od_route_t*
od_routepool_match(od_routepool_t*, od_routeid_t*, od_configroute_t*);

od_server_t*
od_routepool_next(od_routepool_t*, od_serverstate_t);

od_server_t*
od_routepool_server_foreach(od_routepool_t*, od_serverstate_t,
                            od_serverpool_cb_t, void*);

od_client_t*
od_routepool_client_foreach(od_routepool_t*, od_clientstate_t,
                            od_clientpool_cb_t, void*);

int od_routepool_stat_database(od_routepool_t *pool,
                               od_routepool_stat_database_cb_t,
                               uint64_t,
                               void*);

int od_routepool_stat(od_routepool_t *pool,
                      od_routepool_stat_cb_t,
                      uint64_t,
                      void*);

#endif /* OD_ROUTE_POOL_H */
