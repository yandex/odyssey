#ifndef OD_BACKEND_H
#define OD_BACKEND_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

od_server_t*
od_backend_new(od_router_t*, od_route_t*);

#endif /* OD_BACKEND_H */
