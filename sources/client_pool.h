#ifndef ODYSSEY_CLIENT_POOL_H
#define ODYSSEY_CLIENT_POOL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_client_pool od_client_pool_t;

typedef int (*od_client_pool_cb_t)(od_client_t*, void*);

struct od_client_pool
{
	od_list_t active;
	od_list_t queue;
	od_list_t pending;
	int       count_active;
	int       count_queue;
	int       count_pending;
};

void od_client_pool_init(od_client_pool_t*);
void od_client_pool_set(od_client_pool_t*, od_client_t*,
                        od_client_state_t);
od_client_t*
od_client_pool_next(od_client_pool_t*, od_client_state_t);

od_client_t*
od_client_pool_foreach(od_client_pool_t*,
                       od_client_state_t,
                       od_client_pool_cb_t,
                       void*);

static inline int
od_client_pool_total(od_client_pool_t *pool)
{
	return pool->count_active + pool->count_queue +
	       pool->count_pending;
}

#endif /* ODYSSEY_CLIENT_POOL_H */
