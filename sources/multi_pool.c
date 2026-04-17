/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <multi_pool.h>

static inline void key_init(od_multi_pool_key_t *key)
{
	key->dbname = NULL;
	key->username = NULL;
	od_address_init(&key->address);
}

static inline void key_destroy(od_multi_pool_key_t *key)
{
	od_free(key->dbname);
	od_free(key->username);
	od_address_destroy(&key->address);
}

static inline int key_copy(od_multi_pool_key_t *dst,
			   const od_multi_pool_key_t *src)
{
	key_destroy(dst);
	key_init(dst);

	if (src->dbname != NULL) {
		dst->dbname = strdup(src->dbname);
		if (dst->dbname == NULL) {
			goto error;
		}
	}

	if (src->username != NULL) {
		dst->username = strdup(src->username);
		if (dst->username == NULL) {
			goto error;
		}
	}

	if (od_address_copy(&dst->address, &src->address) != OK_RESPONSE) {
		goto error;
	}

	return 0;

error:
	key_destroy(dst);
	key_init(dst);
	return 1;
}

static inline int null_strcmp(const char *a, const char *b)
{
	if (a == NULL && b == NULL) {
		return 0;
	}

	if (a == NULL) {
		return -1;
	}

	if (b == NULL) {
		return 1;
	}

	return strcmp(a, b);
}

static inline int key_cmp(const od_multi_pool_key_t *a,
			  const od_multi_pool_key_t *b)
{
	int rc;

	rc = null_strcmp(a->dbname, b->dbname);
	if (rc != 0) {
		return rc;
	}

	rc = null_strcmp(a->username, b->username);
	if (rc != 0) {
		return rc;
	}

	return od_address_cmp(&a->address, &b->address);
}

static inline od_multi_pool_element_t *
od_multi_pool_element_create(atomic_uint_fast64_t *version)
{
	od_multi_pool_element_t *element =
		od_malloc(sizeof(od_multi_pool_element_t));
	if (element == NULL) {
		return NULL;
	}

	key_init(&element->key);
	od_server_pool_init(&element->pool);
	od_list_init(&element->link);
	mm_wait_list_init(&element->wait_bus, version);

	return element;
}

static inline void od_multi_pool_element_free(od_multi_pool_element_t *element,
					      od_server_pool_free_fn_t free_fn)
{
	key_destroy(&element->key);
	free_fn(&element->pool);
	mm_wait_list_destroy(&element->wait_bus);
	od_free(element);
}

od_multi_pool_t *od_multi_pool_create(od_server_pool_free_fn_t free_fn)
{
	od_multi_pool_t *mpool = od_malloc(sizeof(od_multi_pool_t));
	if (mpool == NULL) {
		return NULL;
	}

	atomic_init(&mpool->version, 0);

	mpool->pool_free_fn = free_fn;
	od_list_init(&mpool->pools);
	pthread_spin_init(&mpool->lock, PTHREAD_PROCESS_PRIVATE);

	return mpool;
}

void od_multi_pool_destroy(od_multi_pool_t *mpool)
{
	od_list_t *i, *s;
	od_list_foreach_safe (&mpool->pools, i, s) {
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);
		od_list_unlink(&el->link);
		od_multi_pool_element_free(el, mpool->pool_free_fn);
	}

	pthread_spin_destroy(&mpool->lock);
	od_free(mpool);
}

static inline od_multi_pool_element_t *
od_multi_pool_get_internal(od_multi_pool_t *mpool,
			   const od_multi_pool_key_t *key)
{
	od_list_t *i;
	od_list_foreach (&mpool->pools, i) {
		od_multi_pool_element_t *element;
		element = od_container_of(i, od_multi_pool_element_t, link);
		if (key_cmp(&element->key, key) == 0) {
			return element;
		}
	}

	return NULL;
}

od_multi_pool_element_t *
od_multi_pool_get_or_create_locked(od_multi_pool_t *mpool,
				   const od_multi_pool_key_t *key)
{
	od_multi_pool_element_t *el = NULL;

	el = od_multi_pool_get_internal(mpool, key);

	if (el == NULL) {
		od_multi_pool_element_t *new_el =
			od_multi_pool_element_create(&mpool->version);
		if (key_copy(&new_el->key, key) != 0) {
			od_multi_pool_element_free(new_el, mpool->pool_free_fn);
			return NULL;
		}
		od_list_append(&mpool->pools, &new_el->link);
		el = new_el;
	}

	return el;
}

od_server_t *
od_multi_pool_foreach_locked(od_multi_pool_t *mpool,
			     const od_multi_pool_key_filter_t filter,
			     void *farg, od_server_state_t state,
			     od_server_pool_cb_t callback, void **argv)
{
	od_list_t *i;
	od_list_foreach (&mpool->pools, i) {
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);

		if (filter != NULL && !filter(farg, &el->key)) {
			continue;
		}

		od_server_t *server = od_server_pool_foreach(&el->pool, state,
							     callback, argv);
		if (server != NULL) {
			return server;
		}
	}

	return NULL;
}

int od_multi_pool_count_active_locked(od_multi_pool_t *mpool,
				      const od_multi_pool_key_filter_t filter,
				      void *farg)
{
	int count = 0;

	od_list_t *i;
	od_list_foreach (&mpool->pools, i) {
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);

		if (filter != NULL && !filter(farg, &el->key)) {
			continue;
		}

		count += od_server_pool_active(&el->pool);
	}

	return count;
}

int od_multi_pool_count_idle_locked(od_multi_pool_t *mpool,
				    const od_multi_pool_key_filter_t filter,
				    void *farg)
{
	int count = 0;

	od_list_t *i;
	od_list_foreach (&mpool->pools, i) {
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);

		if (filter != NULL && !filter(farg, &el->key)) {
			continue;
		}

		count += od_server_pool_idle(&el->pool);
	}

	return count;
}

int od_multi_pool_total_locked(od_multi_pool_t *mpool,
			       const od_multi_pool_key_filter_t filter,
			       void *farg)
{
	int count = 0;

	od_list_t *i;
	od_list_foreach (&mpool->pools, i) {
		od_multi_pool_element_t *el;
		el = od_container_of(i, od_multi_pool_element_t, link);

		if (filter != NULL && !filter(farg, &el->key)) {
			continue;
		}

		count += od_server_pool_total(&el->pool);
	}

	return count;
}

od_server_t *od_multi_pool_peek_any_locked(od_multi_pool_t *mpool,
					   od_server_state_t state)
{
	od_server_t *server = NULL;

	od_list_t *i;
	od_list_foreach (&mpool->pools, i) {
		od_multi_pool_element_t *el =
			od_container_of(i, od_multi_pool_element_t, link);
		server = od_pg_server_pool_next(&el->pool, state);
		if (server != NULL) {
			break;
		}
	}

	return server;
}

typedef struct {
	od_client_t *client;
} server_awaiter_t;

static void attach_freed_server_now(void *private, void *arg)
{
	server_awaiter_t *awaiter = private;
	od_server_t *server = arg;

	if (awaiter == NULL || server == NULL) {
		return;
	}

	if (server->state != OD_SERVER_IDLE) {
		return;
	}

	od_client_t *client = awaiter->client;
	od_server_attach_client(server, client);
}

void od_multi_pool_signal_locked(od_multi_pool_t *mpool,
				 od_multi_pool_element_t *e,
				 od_server_t *server)
{
	if (e != NULL && server != NULL && server->state != OD_SERVER_UNDEF) {
		atomic_fetch_add(&mpool->version, 1);
		mm_wait_list_notify_cb(&e->wait_bus, attach_freed_server_now,
				       server);
		return;
	}

	od_list_t *i;
	od_list_foreach (&mpool->pools, i) {
		od_multi_pool_element_t *el =
			od_container_of(i, od_multi_pool_element_t, link);

		atomic_fetch_add(&mpool->version, 1);
		mm_wait_list_notify(&el->wait_bus);
	}
}

int od_multi_pool_wait(od_multi_pool_element_t *el, od_client_t *client,
		       uint64_t version, uint32_t timeout_ms)
{
	server_awaiter_t awaiter;
	awaiter.client = client;

	return mm_wait_list_compare_wait(&el->wait_bus, &awaiter, version,
					 timeout_ms);
}
