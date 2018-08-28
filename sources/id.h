#ifndef ODYSSEY_ID_H
#define ODYSSEY_ID_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

typedef struct od_id     od_id_t;
typedef struct od_id_mgr od_id_mgr_t;

#define OD_ID_SEEDMAX 6

struct od_id
{
	char    *id_prefix;
	char     id[OD_ID_SEEDMAX * 2];
	uint64_t id_a;
	uint64_t id_b;
};

struct od_id_mgr
{
	struct drand48_data rand_state;
};

void od_id_mgr_init(od_id_mgr_t*);
int  od_id_mgr_seed(od_id_mgr_t*);
void od_id_mgr_generate(od_id_mgr_t*, od_id_t*, char*);

static inline int
od_id_mgr_cmp(od_id_t *a, od_id_t *b)
{
	return memcmp(a->id, b->id, sizeof(a->id)) == 0;
}

#endif /* ODYSSEY_ID_H */
