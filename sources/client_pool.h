#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef int (*od_client_pool_cb_t)(od_client_t *, void **);

struct od_client_pool {
	machine_list_t active;
	machine_list_t queue;
	machine_list_t pending;
	int count_active;
	int count_queue;
	int count_pending;
};

static inline void od_client_pool_init(od_client_pool_t *pool)
{
	pool->count_active = 0;
	pool->count_queue = 0;
	pool->count_pending = 0;
	machine_list_init(&pool->active);
	machine_list_init(&pool->queue);
	machine_list_init(&pool->pending);
}

static inline void od_client_pool_set(od_client_pool_t *pool,
				      od_client_t *client,
				      od_client_state_t state)
{
	if (client->state == state)
		return;
	switch (client->state) {
	case OD_CLIENT_UNDEF:
		break;
	case OD_CLIENT_ACTIVE:
		pool->count_active--;
		break;
	case OD_CLIENT_QUEUE:
		pool->count_queue--;
		break;
	case OD_CLIENT_PENDING:
		pool->count_pending--;
		break;
	}
	machine_list_t *target = NULL;
	switch (state) {
	case OD_CLIENT_UNDEF:
		break;
	case OD_CLIENT_ACTIVE:
		target = &pool->active;
		pool->count_active++;
		break;
	case OD_CLIENT_QUEUE:
		target = &pool->queue;
		pool->count_queue++;
		break;
	case OD_CLIENT_PENDING:
		target = &pool->pending;
		pool->count_pending++;
		break;
	}
	machine_list_unlink(&client->link_pool);
	machine_list_init(&client->link_pool);
	if (target)
		machine_list_append(target, &client->link_pool);
	client->state = state;
}

static inline od_client_t *od_client_pool_next(od_client_pool_t *pool,
					       od_client_state_t state)
{
	int target_count = 0;
	machine_list_t *target = NULL;
	switch (state) {
	case OD_CLIENT_ACTIVE:
		target = &pool->active;
		target_count = pool->count_active;
		break;
	case OD_CLIENT_QUEUE:
		target = &pool->queue;
		target_count = pool->count_queue;
		break;
	case OD_CLIENT_PENDING:
		target = &pool->pending;
		target_count = pool->count_pending;
		break;
	case OD_CLIENT_UNDEF:
		assert(0);
		break;
	}
	if (target_count == 0)
		return NULL;
	od_client_t *client;
	client = machine_container_of(target->next, od_client_t, link_pool);
	return client;
}

static inline od_client_t *od_client_pool_foreach(od_client_pool_t *pool,
						  od_client_state_t state,
						  od_client_pool_cb_t callback,
						  void **argv)
{
	machine_list_t *target = NULL;
	switch (state) {
	case OD_CLIENT_ACTIVE:
		target = &pool->active;
		break;
	case OD_CLIENT_QUEUE:
		target = &pool->queue;
		break;
	case OD_CLIENT_PENDING:
		target = &pool->pending;
		break;
	case OD_CLIENT_UNDEF:
		assert(0);
		break;
	}
	od_client_t *client;
	machine_list_t *i, *n;
	machine_list_foreach_safe(target, i, n)
	{
		client = machine_container_of(i, od_client_t, link_pool);
		int rc;
		rc = callback(client, argv);
		if (rc) {
			return client;
		}
	}
	return NULL;
}

static inline int od_client_pool_total(od_client_pool_t *pool)
{
	return pool->count_active + pool->count_queue + pool->count_pending;
}
