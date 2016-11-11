#ifndef OD_SERVER_POOL_H_
#define OD_SERVER_POOL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odserver_pool_t odserver_pool_t;

struct odserver_pool_t {
	odlist_t active;
	odlist_t connect;
	odlist_t idle;
	int      count_active;
	int      count_connect;
	int      count_idle;
	odlist_t link;
};

void od_serverpool_init(odserver_pool_t*);
void od_serverpool_free(odserver_pool_t*);

odserver_t*
od_serverpool_pop(odserver_pool_t*);

void od_serverpool_set(odserver_pool_t*, odserver_t*,
                       odserver_state_t);

#endif
