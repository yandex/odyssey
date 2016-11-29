#ifndef OD_CLIENT_POOL_H_
#define OD_CLIENT_POOL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_clientpool_t od_clientpool_t;

struct od_clientpool_t {
	od_list_t list;
	int       count;
};

void od_clientpool_init(od_clientpool_t*);
void od_clientpool_free(od_clientpool_t*);

od_client_t*
od_clientpool_new(od_clientpool_t*);

void
od_clientpool_unlink(od_clientpool_t*, od_client_t*);

#endif
