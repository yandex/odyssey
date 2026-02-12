/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <multi_pool.h>

static inline od_multi_pool_element_t *od_multi_pool_element_create(void)
{
	od_multi_pool_element_t *element =
		od_malloc(sizeof(od_multi_pool_element_t));
	if (element == NULL) {
		return NULL;
	}

	od_address_init(&element->address);
	od_server_pool_init(&element->pool);
	od_list_init(&element->link);

	return element;
}

static inline void od_multi_pool_element_free(od_multi_pool_element_t *element,
					      od_server_pool_free_fn_t free_fn)
{
	od_address_destroy(&element->address);
	free_fn(&element->pool);
	od_free(element);
}

od_multi_pool_t *od_multi_pool_create(od_server_pool_free_fn_t free_fn)
{
	od_multi_pool_t *mpool = od_malloc(sizeof(od_multi_pool_t));
	if (mpool == NULL) {
		return NULL;
	}

	mpool->pool_free_fn = free_fn;
	od_list_init(&mpool->pools);
	pthread_spin_init(&mpool->lock, PTHREAD_PROCESS_PRIVATE);

	return mpool;
}

void od_multi_pool_destroy(od_multi_pool_t *mpool)
{
	od_list_t *i, *s;
	od_list_foreach_safe(&mpool->pools, i, s)
	{
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);
		od_list_unlink(&el->link);
		od_multi_pool_element_free(el, mpool->pool_free_fn);
	}

	pthread_spin_destroy(&mpool->lock);
	od_free(mpool);
}

static inline od_multi_pool_element_t *
od_multi_pool_get_internal(od_multi_pool_t *mpool, const od_address_t *address)
{
	od_list_t *i;
	od_list_foreach(&mpool->pools, i)
	{
		od_multi_pool_element_t *element;
		element = od_container_of(i, od_multi_pool_element_t, link);
		if (od_address_cmp(&element->address, address) == 0) {
			return element;
		}
	}

	return NULL;
}

static inline od_multi_pool_element_t *
od_multi_pool_add_internal(od_multi_pool_t *mpool, const od_address_t *address)
{
	od_multi_pool_element_t *new_el = od_multi_pool_element_create();

	int rc = od_address_copy(&new_el->address, address);
	if (rc != OK_RESPONSE) {
		od_multi_pool_element_free(new_el, mpool->pool_free_fn);
		return NULL;
	}

	pthread_spin_lock(&mpool->lock);

	od_multi_pool_element_t *element =
		od_multi_pool_get_internal(mpool, address);
	if (element == NULL) {
		od_list_append(&mpool->pools, &new_el->link);
		element = new_el;

		/* no need to free new_el */
		new_el = NULL;
	} else {
		/*
		 * element was created while we perfromed allocation
		 * need to free new_el
		 */
	}

	pthread_spin_unlock(&mpool->lock);

	if (new_el != NULL) {
		od_multi_pool_element_free(new_el, mpool->pool_free_fn);
	}

	return element;
}

od_multi_pool_element_t *
od_multi_pool_get_or_create(od_multi_pool_t *mpool, const od_address_t *address)
{
	od_multi_pool_element_t *el = NULL;

	pthread_spin_lock(&mpool->lock);
	el = od_multi_pool_get_internal(mpool, address);
	pthread_spin_unlock(&mpool->lock);

	if (el == NULL) {
		el = od_multi_pool_add_internal(mpool, address);
	}

	return el;
}

od_server_t *od_multi_pool_foreach(od_multi_pool_t *mpool,
				   od_server_state_t state,
				   od_server_pool_cb_t callback, void **argv)
{
	pthread_spin_lock(&mpool->lock);

	od_list_t *i;
	od_list_foreach(&mpool->pools, i)
	{
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);
		od_server_t *server = od_server_pool_foreach(&el->pool, state,
							     callback, argv);
		if (server != NULL) {
			pthread_spin_unlock(&mpool->lock);
			return server;
		}
	}

	pthread_spin_unlock(&mpool->lock);

	return NULL;
}

int od_multi_pool_count_active(od_multi_pool_t *mpool)
{
	int count = 0;

	pthread_spin_lock(&mpool->lock);

	od_list_t *i;
	od_list_foreach(&mpool->pools, i)
	{
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);
		count += od_server_pool_active(&el->pool);
	}

	pthread_spin_unlock(&mpool->lock);

	return count;
}

int od_multi_pool_count_idle(od_multi_pool_t *mpool)
{
	int count = 0;

	pthread_spin_lock(&mpool->lock);

	od_list_t *i;
	od_list_foreach(&mpool->pools, i)
	{
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);
		count += od_server_pool_idle(&el->pool);
	}

	pthread_spin_unlock(&mpool->lock);

	return count;
}

int od_multi_pool_total(od_multi_pool_t *mpool)
{
	int count = 0;

	pthread_spin_lock(&mpool->lock);

	od_list_t *i;
	od_list_foreach(&mpool->pools, i)
	{
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);
		count += od_server_pool_total(&el->pool);
	}

	pthread_spin_unlock(&mpool->lock);

	return count;
}
