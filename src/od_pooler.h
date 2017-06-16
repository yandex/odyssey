#ifndef OD_POOLER_H
#define OD_POOLER_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_pooler od_pooler_t;

struct od_pooler
{
	int64_t        machine;
	machine_tls_t *tls;
	uint64_t       client_seq;
	od_system_t   *system;
};

int od_pooler_init(od_pooler_t*, od_system_t*);
int od_pooler_start(od_pooler_t*);

#endif /* OD_INSTANCE_H */
