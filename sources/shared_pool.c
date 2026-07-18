/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <shared_pool.h>
#include <multi_pool.h>
#include <server_pool.h>
#include <od_memory.h>

static inline void od_shared_pool_free(od_shared_pool_t *sp)
{
	od_list_unlink(&sp->link);

	od_multi_pool_destroy(sp->mpool);

	od_free(sp->name);

	mm_spinlock_destroy(&sp->lock);

	od_free(sp);
}

od_shared_pool_t *od_shared_pool_create(const char *name)
{
	od_shared_pool_t *sp = od_malloc(sizeof(od_shared_pool_t));
	if (sp == NULL) {
		return NULL;
	}
	memset(sp, 0, sizeof(od_shared_pool_t));

	sp->name = strdup(name);
	if (sp->name == NULL) {
		od_free(sp);
		return NULL;
	}

	sp->refs = 1;
	sp->mpool = od_multi_pool_create(od_pg_server_pool_free);
	if (sp->mpool == NULL) {
		od_free(sp->name);
		od_free(sp);
		return NULL;
	}

	od_list_init(&sp->link);

	mm_spinlock_init(&sp->lock);

	return sp;
}

od_shared_pool_t *od_shared_pool_ref(od_shared_pool_t *sp)
{
	mm_spinlock_lock(&sp->lock);
	++sp->refs;
	mm_spinlock_unlock(&sp->lock);

	return sp;
}

void od_shared_pool_unref(od_shared_pool_t *sp)
{
	int refs;

	mm_spinlock_lock(&sp->lock);
	refs = sp->refs--;
	mm_spinlock_unlock(&sp->lock);

	if (refs == 1) {
		od_shared_pool_free(sp);
	}
}
