#ifndef OD_ROUTER_H
#define OD_ROUTER_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_router od_router_t;

struct od_router
{
	int64_t      machine;
	od_system_t *system;
};

void od_router_init(od_router_t*, od_system_t*);
int  od_router_start(od_router_t*);

#endif /* OD_ROUTER_H */
