#ifndef OD_ROUTER_H
#define OD_ROUTER_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_router od_router_t;

typedef enum
{
	OD_ROK,
	OD_RERROR,
	OD_RERROR_NOT_FOUND,
	OD_RERROR_LIMIT
} od_routerstatus_t;

struct od_router
{
	int64_t         machine;
	od_routepool_t  route_pool;
	machine_queue_t queue;
	od_system_t    *system;
};

int od_router_init(od_router_t*, od_system_t*);
int od_router_start(od_router_t*);

od_routerstatus_t
od_route(od_router_t*, od_client_t*);

#endif /* OD_ROUTER_H */
