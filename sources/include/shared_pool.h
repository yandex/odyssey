#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <pthread.h>

#include <types.h>
#include <list.h>

struct od_shared_pool {
	od_multi_pool_t *mpool;
	pthread_spinlock_t lock;
	char *name;
	od_list_t link;
	int pool_size; /* TODO: use full od_rule_pool_t here? */
	int refs;
};

od_shared_pool_t *od_shared_pool_create(const char *name);
od_shared_pool_t *od_shared_pool_ref(od_shared_pool_t *sp);
void od_shared_pool_unref(od_shared_pool_t *sp);
