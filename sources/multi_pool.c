/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

void od_multi_pool_element_init(od_multi_pool_element_t *element)
{
	od_address_init(&element->key);
	od_server_pool_init(&element->pool);
}

void od_multi_pool_element_destroy(od_multi_pool_element_t *element,
				   od_server_pool_free_fn_t free_fn)
{
	od_address_destroy(&element->key);
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
	free(mpool);
}

od_server_pool_t *od_multi_pool_get_internal(od_multi_pool_t *mpool,
					     const od_address_t *address)
{
	od_server_pool_t *pool = NULL;

	for (size_t i = 0; i < mpool->size; ++i) {
		od_multi_pool_element_t *element = &mpool->pools[i];
		if (od_address_cmp(&element->key, address) == 0) {
			pool = &element->pool;
			break;
		}
	}

	return pool;
}

int od_multi_pool_add(od_multi_pool_t *mpool, const od_address_t *address)
{
	pthread_spin_lock(&mpool->lock);

	if (mpool->size == mpool->capacity) {
		return NOT_OK_RESPONSE;
	}

	od_server_pool_t *pool = od_multi_pool_get_internal(mpool, address);
	if (pool != NULL) {
		return OK_RESPONSE;
	}

	od_multi_pool_element_t *element = &mpool->pools[mpool->size];

	int rc = od_address_copy(&element->key, address);
	if (rc != OK_RESPONSE) {
		return rc;
	}

	++mpool->size;

	pthread_spin_unlock(&mpool->lock);

	return OK_RESPONSE;
}

od_server_pool_t *od_multi_pool_get(od_multi_pool_t *mpool,
				    const od_address_t *address)
{
	od_server_pool_t *pool = NULL;

	pthread_spin_lock(&mpool->lock);

	pool = od_multi_pool_get_internal(mpool, address);

	pthread_spin_unlock(&mpool->lock);

	return pool;
}