#ifndef ODYSSEY_SERVER_POOL_H
#define ODYSSEY_SERVER_POOL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_server_pool od_server_pool_t;

typedef int (*od_server_pool_cb_t)(od_server_t*, void*);

struct od_server_pool
{
	od_list_t active;
	od_list_t idle;
	od_list_t expire;
	int       count_active;
	int       count_idle;
	int       count_expire;
	od_list_t link;
};

void od_server_pool_init(od_server_pool_t*);
void od_server_pool_free(od_server_pool_t*);
void od_server_pool_set(od_server_pool_t*, od_server_t*,
                        od_server_state_t);

od_server_t*
od_server_pool_next(od_server_pool_t*, od_server_state_t);

od_server_t*
od_server_pool_foreach(od_server_pool_t*, od_server_state_t,
                       od_server_pool_cb_t, void*);

static inline int
od_server_pool_total(od_server_pool_t *pool)
{
	return pool->count_active +
	       pool->count_idle +
	       pool->count_expire;
}

#endif /* ODYSSEY_SERVER_POOL_H */
