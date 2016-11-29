#ifndef OD_CLIENT_POOL_H_
#define OD_CLIENT_POOL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odclient_pool_t odclient_pool_t;

struct odclient_pool_t {
	od_list_t list;
	int       count;
};

void od_clientpool_init(odclient_pool_t*);
void od_clientpool_free(odclient_pool_t*);

odclient_t*
od_clientpool_new(odclient_pool_t*);

void
od_clientpool_unlink(odclient_pool_t*, odclient_t*);

#endif
