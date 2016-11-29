#ifndef OD_SERVER_POOL_H_
#define OD_SERVER_POOL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odserver_pool_t odserver_pool_t;

typedef int (*odserver_pool_cb_t)(od_server_t*, void*);

struct odserver_pool_t {
	od_list_t active;
	od_list_t connect;
	od_list_t reset;
	od_list_t expire;
	od_list_t idle;
	int       count_active;
	int       count_connect;
	int       count_reset;
	int       count_expire;
	int       count_idle;
	od_list_t link;
};

void od_serverpool_init(odserver_pool_t*);
void od_serverpool_free(odserver_pool_t*);
void od_serverpool_set(odserver_pool_t*, od_server_t*,
                       od_serverstate_t);

od_server_t*
od_serverpool_pop(odserver_pool_t*, od_serverstate_t);

od_server_t*
od_serverpool_foreach(odserver_pool_t*, od_serverstate_t,
                      odserver_pool_cb_t, void*);

#endif
