#ifndef OD_ROUTER_CANCEL_H
#define OD_ROUTER_CANCEL_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct
{
	od_id_t             id;
	od_configstorage_t *config;
	shapito_key_t       key;
} od_routercancel_t;

static inline void
od_routercancel_init(od_routercancel_t *cancel)
{
	cancel->config = NULL;
	shapito_key_init(&cancel->key);
}

static inline void
od_routercancel_free(od_routercancel_t *cancel)
{
	if (cancel->config)
		od_configstorage_free(cancel->config);
}

#endif /* OD_ROUTER_CANCEL_H */
