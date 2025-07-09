/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

void od_multi_pool_element_init(od_multi_pool_element_t *element)
{
	od_address_init(&element->address);
	od_server_pool_init(&element->pool);
}

void od_multi_pool_element_destroy(od_multi_pool_element_t *element,
				   od_server_pool_free_fn_t free_fn)
{
	od_address_destroy(&element->address);
	free_fn(&element->pool);
}

od_multi_pool_t *od_multi_pool_create(size_t max_keys,
				      od_server_pool_free_fn_t free_fn)
{
	od_multi_pool_t *mpool = malloc(sizeof(od_multi_pool_t));
	if (mpool == NULL) {
		return NULL;
	}

	mpool->size = 0;
	mpool->capacity = max_keys;
	mpool->pool_free_fn = free_fn;
	pthread_spin_init(&mpool->lock, PTHREAD_PROCESS_PRIVATE);

	mpool->pools =
		malloc(mpool->capacity * sizeof(od_multi_pool_element_t));
	if (mpool->pools == NULL) {
		free(mpool);
		return NULL;
	}

	for (size_t i = 0; i < mpool->capacity; ++i) {
		od_multi_pool_element_init(&mpool->pools[i]);
	}

	return mpool;
}

void od_multi_pool_destroy(od_multi_pool_t *mpool)
{
	for (size_t i = 0; i < mpool->capacity; ++i) {
		od_multi_pool_element_destroy(&mpool->pools[i],
					      mpool->pool_free_fn);
	}

	pthread_spin_destroy(&mpool->lock);
	free(mpool->pools);
	free(mpool);
}

od_multi_pool_element_t *od_multi_pool_get_internal(od_multi_pool_t *mpool,
						    const od_address_t *address)
{
	for (size_t i = 0; i < mpool->size; ++i) {
		od_multi_pool_element_t *element = &mpool->pools[i];
		if (od_address_cmp(&element->address, address) == 0) {
			return element;
		}
	}

	return NULL;
}

static inline od_multi_pool_element_t *
od_multi_pool_add_internal(od_multi_pool_t *mpool, const od_address_t *address)
{
	od_multi_pool_element_t *element = &mpool->pools[mpool->size];

	int rc = od_address_copy(&element->address, address);
	if (rc != OK_RESPONSE) {
		return NULL;
	}

	++mpool->size;

	return element;
}

od_multi_pool_element_t *od_multi_pool_get(od_multi_pool_t *mpool,
					   const od_address_t *address)
{
	od_multi_pool_element_t *el = NULL;

	pthread_spin_lock(&mpool->lock);

	el = od_multi_pool_get_internal(mpool, address);

	pthread_spin_unlock(&mpool->lock);

	return el;
}

od_multi_pool_element_t *
od_multi_pool_get_or_create(od_multi_pool_t *mpool, const od_address_t *address)
{
	od_multi_pool_element_t *el = NULL;

	pthread_spin_lock(&mpool->lock);

	el = od_multi_pool_get_internal(mpool, address);
	if (el == NULL) {
		el = od_multi_pool_add_internal(mpool, address);
	}

	pthread_spin_unlock(&mpool->lock);

	return el;
}

od_server_t *od_multi_pool_foreach(od_multi_pool_t *mpool,
				   od_server_state_t state,
				   od_server_pool_cb_t callback, void **argv)
{
	for (size_t i = 0; i < mpool->size; ++i) {
		od_server_t *server = od_server_pool_foreach(
			&mpool->pools[i].pool, state, callback, argv);
		if (server != NULL) {
			return server;
		}
	}

	return NULL;
}

int od_multi_pool_count_active(od_multi_pool_t *mpool)
{
	int count = 0;

	for (size_t i = 0; i < mpool->size; ++i) {
		count += mpool->pools[i].pool.count_active;
	}

	return count;
}

int od_multi_pool_count_idle(od_multi_pool_t *mpool)
{
	int count = 0;

	for (size_t i = 0; i < mpool->size; ++i) {
		count += mpool->pools[i].pool.count_idle;
	}

	return count;
}

int od_multi_pool_total(od_multi_pool_t *mpool)
{
	int count = 0;

	for (size_t i = 0; i < mpool->size; ++i) {
		count += od_server_pool_total(&mpool->pools[i].pool);
	}

	return count;
}
