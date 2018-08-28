#ifndef ODYSSEY_ROUTER_CANCEL_H
#define ODYSSEY_ROUTER_CANCEL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct
{
	od_id_t              id;
	od_config_storage_t *config;
	kiwi_key_t           key;
} od_router_cancel_t;

static inline void
od_router_cancel_init(od_router_cancel_t *cancel)
{
	cancel->config = NULL;
	kiwi_key_init(&cancel->key);
}

static inline void
od_router_cancel_free(od_router_cancel_t *cancel)
{
	if (cancel->config)
		od_config_storage_free(cancel->config);
}

#endif /* ODYSSEY_ROUTER_CANCEL_H */
