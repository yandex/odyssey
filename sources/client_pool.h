#ifndef OD_CLIENT_POOL_H
#define OD_CLIENT_POOL_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_clientpool od_clientpool_t;

typedef int (*od_clientpool_cb_t)(od_client_t*, void*);

struct od_clientpool
{
	od_list_t active;
	od_list_t queue;
	od_list_t pending;
	int       count_active;
	int       count_queue;
	int       count_pending;
};

void od_clientpool_init(od_clientpool_t*);
void od_clientpool_set(od_clientpool_t*, od_client_t*,
                       od_clientstate_t);
od_client_t*
od_clientpool_next(od_clientpool_t*, od_clientstate_t);

od_client_t*
od_clientpool_foreach(od_clientpool_t*,
                      od_clientstate_t,
                      od_clientpool_cb_t,
                      void*);

static inline int
od_clientpool_total(od_clientpool_t *pool) {
	return pool->count_active + pool->count_queue +
	       pool->count_pending;
}

#endif /* OD_CLIENT_POOL_H */
