#ifndef OD_CANCEL_H
#define OD_CANCEL_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int od_cancel(od_system_t*, od_schemestorage_t*, shapito_key_t*, od_id_t*);
int od_cancel_match(od_system_t*, od_routepool_t*, shapito_key_t*);

#endif
