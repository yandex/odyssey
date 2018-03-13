#ifndef OD_CANCEL_H
#define OD_CANCEL_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int od_cancel(od_global_t*, shapito_stream_t*, od_configstorage_t*, shapito_key_t*, od_id_t*);
int od_cancel_find(od_routepool_t*, shapito_key_t*, od_routercancel_t*);

#endif
