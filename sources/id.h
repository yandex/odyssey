#ifndef OD_ID_H
#define OD_ID_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_id    od_id_t;
typedef struct od_idmgr od_idmgr_t;

#define OD_ID_SEEDMAX 6

struct od_id
{
	char    *id_prefix;
	char     id[OD_ID_SEEDMAX * 2];
	uint64_t id_a;
	uint64_t id_b;
};

struct od_idmgr
{
	struct drand48_data rand_state;
};

void od_idmgr_init(od_idmgr_t*);
int  od_idmgr_seed(od_idmgr_t*);
void od_idmgr_generate(od_idmgr_t*, od_id_t*, char*);

static inline int
od_idmgr_cmp(od_id_t *a, od_id_t *b)
{
	return memcmp(a->id, b->id, sizeof(a->id)) == 0;
}

#endif /* OD_ID_H */
