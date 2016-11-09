#ifndef OD_POOL_H_
#define OD_POOL_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct odpool_t odpool_t;

struct odpool_t {
	odlist_t active;
	odlist_t connect;
	odlist_t idle;
	int      count_active;
	int      count_connect;
	int      count_idle;
	/* database, user */
	odlist_t link;
};

void od_poolinit(odpool_t*);
void od_poolfree(odpool_t*);

static inline odpool_t*
od_poolalloc(void)
{
	odpool_t *p = malloc(sizeof(*p));
	if (p == NULL)
		return NULL;
	od_poolinit(p);
	return p;
}

odserver_t*
od_poolpop(odpool_t*);

void od_poolset(odpool_t*, odserver_t*, odserver_state_t);

#endif
