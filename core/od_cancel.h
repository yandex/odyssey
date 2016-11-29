#ifndef OD_CANCEL_H_
#define OD_CANCEL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_cancel(od_pooler_t*, so_key_t*);
int od_cancel_of(od_pooler_t*, od_schemeserver_t*, so_key_t*);

#endif
