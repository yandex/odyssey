#ifndef OD_CLIENT_POOL_H_
#define OD_CLIENT_POOL_H_

/*
 * odissey.
 *
 * PostgreSQL connection per and request router.
*/

typedef struct od_clientpool_t od_clientpool_t;

struct od_clientpool_t {
	od_list_t active;
	od_list_t queue;
	int       count_active;
	int       count_queue;
};

void od_clientpool_init(od_clientpool_t*);
void od_clientpool_set(od_clientpool_t*, od_client_t*,
                       od_clientstate_t);
od_client_t*
od_clientpool_next(od_clientpool_t*, od_clientstate_t);

#endif
