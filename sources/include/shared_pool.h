#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <pthread.h>

#include <types.h>
#include <list.h>

struct od_shared_pool_element {
	od_multi_pool_t *pool;
	char *username;
	char *dbname;
};

struct od_shared_pool {
	od_shared_pool_element_t *elements;
	size_t size;
	size_t capacity;
	pthread_spinlock_t lock;
	char *name;
	od_list_t link;
	int pool_size; /* TODO: use full od_rule_pool_t here? */
	int refs;
};

od_shared_pool_t *od_shared_pool_create(const char *name);
od_shared_pool_t *od_shared_pool_ref(od_shared_pool_t *sp);
void od_shared_pool_unref(od_shared_pool_t *sp);

int od_shared_pool_total(od_shared_pool_t *sp);

od_multi_pool_t *od_shared_pool_get_or_create(od_shared_pool_t *sp,
					      const char *dbname,
					      const char *username);

od_server_t *od_shared_pool_get_idle(od_shared_pool_t *sp, const char *dbname, const char *username);