#ifndef OD_ROUTER_CANCEL_H
#define OD_ROUTER_CANCEL_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct
{
	od_id_t id;
	od_schemestorage_t *scheme;
	shapito_key_t key;
} od_routercancel_t;

static inline void
od_routercancel_init(od_routercancel_t *cancel)
{
	cancel->scheme = NULL;
	shapito_key_init(&cancel->key);
}

static inline void
od_routercancel_free(od_routercancel_t *cancel)
{
	if (cancel->scheme)
		od_schemestorage_free(cancel->scheme);
}

#endif
