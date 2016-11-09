#ifndef OD_POOLER_H_
#define OD_POOLER_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odpooler_t odpooler_t;

struct odpooler_t {
	ft_t             env;
	ftio_t           server;
	odserver_pool_t  pool;
	od_t            *od;
};

int od_pooler_init(odpooler_t*, od_t*);
int od_pooler_start(odpooler_t*);

#endif
