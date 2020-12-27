#ifndef ODYSSEY_SERVER_POOL_H
#define ODYSSEY_SERVER_POOL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef struct od_server_pool od_server_pool_t;

typedef int (*od_server_pool_cb_t)(od_server_t *, void **);

struct od_server_pool {
	od_list_t active;
	od_list_t idle;
	int count_active;
	int count_idle;
};

static inline void od_server_pool_init(od_server_pool_t *pool)
{
	pool->count_active = 0;
	pool->count_idle = 0;
	od_list_init(&pool->idle);
	od_list_init(&pool->active);
}

static inline void od_server_pool_free(od_server_pool_t *pool)
{
	od_server_t *server;
	od_list_t *i, *n;
	od_list_foreach_safe(&pool->idle, i, n)
	{
		server = od_container_of(i, od_server_t, link);
		od_server_free(server);
	}
	od_list_foreach_safe(&pool->active, i, n)
	{
		server = od_container_of(i, od_server_t, link);
		od_server_free(server);
	}
}

static inline void od_server_pool_set(od_server_pool_t *pool,
				      od_server_t *server,
				      od_server_state_t state)
{
	if (server->state == state)
		return;
	switch (server->state) {
	case OD_SERVER_UNDEF:
		break;
	case OD_SERVER_IDLE:
		pool->count_idle--;
		break;
	case OD_SERVER_ACTIVE:
		pool->count_active--;
		break;
	}
	od_list_t *target = NULL;
	switch (state) {
	case OD_SERVER_UNDEF:
		break;
	case OD_SERVER_IDLE:
		target = &pool->idle;
		pool->count_idle++;
		break;
	case OD_SERVER_ACTIVE:
		target = &pool->active;
		pool->count_active++;
		break;
	}
	od_list_unlink(&server->link);
	od_list_init(&server->link);
	if (target)
		od_list_push(target, &server->link);
	server->state = state;
}

static inline od_server_t *od_server_pool_next(od_server_pool_t *pool,
					       od_server_state_t state)
{
	int target_count = 0;
	od_list_t *target = NULL;
	switch (state) {
	case OD_SERVER_IDLE:
		target_count = pool->count_idle;
		target = &pool->idle;
		break;
	case OD_SERVER_ACTIVE:
		target_count = pool->count_active;
		target = &pool->active;
		break;
	case OD_SERVER_UNDEF:
		assert(0);
		break;
	}
	if (target_count == 0)
		return NULL;
	od_server_t *server;
	server = od_container_of(target->next, od_server_t, link);
	return server;
}

static inline od_server_t *od_server_pool_foreach(od_server_pool_t *pool,
						  od_server_state_t state,
						  od_server_pool_cb_t callback,
						  void **argv)
{
	od_list_t *target = NULL;
	switch (state) {
	case OD_SERVER_IDLE:
		target = &pool->idle;
		break;
	case OD_SERVER_ACTIVE:
		target = &pool->active;
		break;
	case OD_SERVER_UNDEF:
		assert(0);
		break;
	}
	od_server_t *server;
	od_list_t *i, *n;
	od_list_foreach_safe(target, i, n)
	{
		server = od_container_of(i, od_server_t, link);
		int rc;
		rc = callback(server, argv);
		if (rc) {
			return server;
		}
	}
	return NULL;
}

static inline int od_server_pool_total(od_server_pool_t *pool)
{
	return pool->count_active + pool->count_idle;
}

#endif /* ODYSSEY_SERVER_POOL_H */
