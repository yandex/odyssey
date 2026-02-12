/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <shared_pool.h>
#include <multi_pool.h>
#include <od_memory.h>
#include <client.h>
#include <server.h>
#include <pool.h>

static inline void od_shared_pool_element_destroy(od_shared_pool_element_t *el)
{
	od_free(el->username);
	od_free(el->dbname);
	od_multi_pool_destroy(el->pool);
}

static inline int od_shared_pool_element_init(od_shared_pool_element_t *el,
					      const char *dbname,
					      const char *username)
{
	memset(el, 0, sizeof(od_shared_pool_element_t));

	el->username = strdup(username);
	if (el->username == NULL) {
		return 1;
	}

	el->dbname = strdup(dbname);
	if (el->dbname == NULL) {
		od_free(el->username);
		return 1;
	}

	el->pool = od_multi_pool_create(OD_STORAGE_MAX_ENDPOINTS,
					od_pg_server_pool_free);
	if (el->pool == NULL) {
		od_free(el->dbname);
		od_free(el->username);
		return 1;
	}

	return 0;
}

static inline int
od_shared_pool_element_match(const od_shared_pool_element_t *el,
			     const char *dbname, const char *username)
{
	if (strcmp(el->username, username) != 0) {
		return 0;
	}

	if (strcmp(el->dbname, dbname) != 0) {
		return 0;
	}

	return 1;
}

static inline void od_shared_pool_free(od_shared_pool_t *sp)
{
	od_list_unlink(&sp->link);

	for (size_t i = 0; i < sp->size; ++i) {
		od_shared_pool_element_destroy(&sp->elements[i]);
	}
	od_free(sp->elements);

	od_free(sp->name);

	pthread_spin_destroy(&sp->lock);

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

	sp->pool_size = 0;
	sp->refs = 1;
	sp->capacity = 256;
	sp->size = 0;
	sp->elements =
		od_malloc(sp->capacity * sizeof(od_shared_pool_element_t));
	if (sp->elements == NULL) {
		od_free(sp->name);
		od_free(sp);
		return NULL;
	}

	od_list_init(&sp->link);

	pthread_spin_init(&sp->lock, PTHREAD_PROCESS_PRIVATE);

	return sp;
}

od_shared_pool_t *od_shared_pool_ref(od_shared_pool_t *sp)
{
	pthread_spin_lock(&sp->lock);
	++sp->refs;
	pthread_spin_unlock(&sp->lock);

	return sp;
}

void od_shared_pool_unref(od_shared_pool_t *sp)
{
	int refs;

	pthread_spin_lock(&sp->lock);
	refs = sp->refs--;
	pthread_spin_unlock(&sp->lock);

	if (refs == 1) {
		od_shared_pool_free(sp);
	}
}

int od_shared_pool_total(od_shared_pool_t *sp)
{
	int total_size = 0;

	pthread_spin_lock(&sp->lock);
	for (size_t i = 0; i < sp->size; ++i) {
		total_size += od_multi_pool_total(sp->elements[i].pool);
	}
	pthread_spin_unlock(&sp->lock);

	return total_size;
}

static inline od_multi_pool_t *od_shared_pool_get_locked(od_shared_pool_t *sp,
							 const char *dbname,
							 const char *username)
{
	for (size_t i = 0; i < sp->size; ++i) {
		od_shared_pool_element_t *el = &sp->elements[i];

		if (od_shared_pool_element_match(el, dbname, username)) {
			return el->pool;
		}
	}

	return NULL;
}

static inline od_multi_pool_t *
od_shared_pool_create_locked(od_shared_pool_t *sp, const char *dbname,
			     const char *username)
{
	if (sp->size == sp->capacity) {
		sp->capacity *= 2;
		od_shared_pool_element_t *new = od_realloc(
			sp->elements,
			sp->capacity * sizeof(od_shared_pool_element_t));
		if (new == NULL) {
			return NULL;
		}
		sp->elements = new;
	}

	if (od_shared_pool_element_init(&sp->elements[sp->size], dbname,
					username)) {
		return NULL;
	}

	return sp->elements[++sp->size].pool;
}

static inline od_multi_pool_t *
od_shared_pool_get_or_create_locked(od_shared_pool_t *sp, const char *dbname,
				    const char *username)
{
	od_multi_pool_t *pool = NULL;

	pool = od_shared_pool_get_locked(sp, dbname, username);

	if (pool != NULL) {
		return pool;
	}

	/* TODO: do not allocate with lock held */
	return od_shared_pool_create_locked(sp, dbname, username);
}

od_server_t *od_shared_pool_try_attach_server(od_shared_pool_t *sp,
					      od_client_t *client)
{
	(void)sp;
	(void)client;
	// int total_size = 0;
	// int pool_size = sp->pool->size;

	// pthread_spin_lock(&sp->lock);
	// for (size_t i = 0; i < sp->size; ++i) {
	// 	total_size += od_multi_pool_total(sp->elements[i].pool);
	// }

	// if (!od_rule_pool_can_add(sp->pool, total_size)) {
	// }

	// pthread_spin_unlock(&sp->lock);

	// if (total)
	return NULL;
}

od_multi_pool_t *od_shared_pool_get_or_create(od_shared_pool_t *sp,
					      const char *dbname,
					      const char *username)
{
	od_multi_pool_t *mpool = NULL;

	pthread_spin_lock(&spool->lock);

	mpool = od_shared_pool_get_or_create_locked(sp, dbname, username);

	pthread_spin_unlock(&spool->link);

	return mpool;
}

od_server_t *od_shared_pool_get_idle(od_shared_pool_t *sp, const char *dbname, const char *username)
{
	pthread_spin_lock(&sp->lock);

	od_multi_pool_t *mpool = NULL;
	mpool = od_shared_pool_get_locked(so, dbname, username);

	if (mpool != NULL) {
		
	}

	pthread_spin_unlock(&sp->lock);
}