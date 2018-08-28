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
	OD_ROK,
	OD_RERROR,
	OD_RERROR_NOT_FOUND,
	OD_RERROR_LIMIT,
	OD_RERROR_TIMEDOUT
} od_router_status_t;

struct od_router
{
	od_route_pool_t    route_pool;
	machine_channel_t *channel;
	int                clients;
	od_global_t       *global;
};

void od_router_init(od_router_t*, od_global_t*);
int  od_router_start(od_router_t*);

od_router_status_t
od_route(od_client_t*);

od_router_status_t
od_unroute(od_client_t*);

od_router_status_t
od_router_attach(od_client_t*);

od_router_status_t
od_router_detach(od_client_t*);

od_router_status_t
od_router_detach_and_unroute(od_client_t*);

od_router_status_t
od_router_close(od_client_t*);

od_router_status_t
od_router_close_and_unroute(od_client_t*);

od_router_status_t
od_router_cancel(od_client_t*, od_router_cancel_t*);

#endif /* ODYSSEY_ROUTER_H */
