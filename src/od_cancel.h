#ifndef OD_CANCEL_H
#define OD_CANCEL_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_cancel(od_system_t*, od_schemeserver_t*, so_key_t*, od_id_t*);
int od_cancel_match(od_system_t*, od_routepool_t*, so_key_t*);

#endif
