#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * address -> pool 'map'
 */

typedef void (*od_server_pool_free_fn_t)(od_server_pool_t *);

struct od_multi_pool_element {
	od_address_t address;
	od_server_pool_t pool;
};

void od_multi_pool_element_init(od_multi_pool_element_t *element);
void od_multi_pool_element_destroy(od_multi_pool_element_t *element,
				   od_server_pool_free_fn_t free_fn);

struct od_multi_pool {
	size_t size;
	size_t capacity;
	od_multi_pool_element_t *pools;
	od_server_pool_free_fn_t pool_free_fn;
	pthread_spinlock_t lock;
};

od_multi_pool_t *od_multi_pool_create(size_t max_keys,
				      od_server_pool_free_fn_t pool_free_fn);
void od_multi_pool_destroy(od_multi_pool_t *mpool);
od_multi_pool_element_t *
od_multi_pool_get_or_create(od_multi_pool_t *mpool,
			    const od_address_t *address);
od_multi_pool_element_t *od_multi_pool_get(od_multi_pool_t *mpool,
					   const od_address_t *address);
od_server_t *od_multi_pool_foreach(od_multi_pool_t *mpool,
				   od_server_state_t state,
				   od_server_pool_cb_t callback, void **argv);
int od_multi_pool_count_active(od_multi_pool_t *mpool);
int od_multi_pool_count_idle(od_multi_pool_t *mpool);
int od_multi_pool_total(od_multi_pool_t *mpool);
