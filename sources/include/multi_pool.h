#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * address -> pool 'map'
 */

#include <stdatomic.h>

#include <machinarium/wait_list.h>

#include <types.h>
#include <address.h>
#include <list.h>
#include <server_pool.h>
#include <server.h>

typedef void (*od_server_pool_free_fn_t)(od_server_pool_t *);

struct od_multi_pool_key {
	char *dbname;
	char *username;
	od_address_t address;
};

struct od_multi_pool_element {
	od_multi_pool_key_t key;
	od_server_pool_t pool;
	od_list_t link;
};

typedef int (*od_multi_pool_element_cb_t)(od_multi_pool_element_t *, void **);

struct od_multi_pool {
	od_list_t pools;
	od_server_pool_free_fn_t pool_free_fn;
	pthread_spinlock_t lock;

	mm_wait_list_t *wait_bus;
	/* should increase every time servers in the route's pool are changed */
	atomic_uint_fast64_t version;
};

static inline void od_multi_pool_lock(od_multi_pool_t *mpool)
{
	pthread_spin_lock(&mpool->lock);
}

static inline void od_multi_pool_unlock(od_multi_pool_t *mpool)
{
	pthread_spin_unlock(&mpool->lock);
}

static inline uint64_t od_multi_pool_version(od_multi_pool_t *mpool)
{
	return atomic_load(&mpool->version);
}

static inline int od_multi_pool_signal(od_multi_pool_t *mpool)
{
	atomic_fetch_add(&mpool->version, 1);
	mm_wait_list_notify(mpool->wait_bus);
	return 0;
}

static inline int od_multi_pool_wait(od_multi_pool_t *mpool, uint64_t version,
				     uint32_t timeout_ms)
{
	int rc;
	rc = mm_wait_list_compare_wait(mpool->wait_bus, version, timeout_ms);
	return rc;
}

od_multi_pool_t *od_multi_pool_create(od_server_pool_free_fn_t pool_free_fn);
void od_multi_pool_destroy(od_multi_pool_t *mpool);
od_multi_pool_element_t *
od_multi_pool_get_or_create(od_multi_pool_t *mpool,
			    const od_multi_pool_key_t *key);

/* return 1 if key fits, 0 otherwise */
typedef int (*od_multi_pool_key_filter_t)(void *, const od_multi_pool_key_t *);

int od_multi_pool_filter_all(void *arg, const od_multi_pool_key_t *key);

od_server_t *od_multi_pool_foreach(od_multi_pool_t *mpool,
				   const od_multi_pool_key_filter_t filter,
				   void *farg, od_server_state_t state,
				   od_server_pool_cb_t callback, void **argv);
int od_multi_pool_count_active(od_multi_pool_t *mpool,
			       const od_multi_pool_key_filter_t filter,
			       void *farg);
int od_multi_pool_count_idle(od_multi_pool_t *mpool,
			     const od_multi_pool_key_filter_t filter,
			     void *farg);
int od_multi_pool_total(od_multi_pool_t *mpool,
			const od_multi_pool_key_filter_t filter, void *farg);
