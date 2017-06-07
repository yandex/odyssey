#ifndef OD_RELAY_POOL_H
#define OD_RELAY_POOL_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_relaypool od_relaypool_t;

struct od_relaypool
{
	od_relay_t *pool;
	int count;
};

int od_relaypool_init(od_relaypool_t*, od_system_t*, int);
int od_relaypool_start(od_relaypool_t*);

#endif /* OD_RELAY_POOL_H */
