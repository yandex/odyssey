#ifndef OD_ID_H
#define OD_ID_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_id    od_id_t;
typedef struct od_idmgr od_idmgr_t;

#define OD_ID_SEEDMAX 6

struct od_id
{
	uint8_t id[OD_ID_SEEDMAX * 2];
};

struct od_idmgr
{
	uint64_t seq;
	uint8_t  seed[OD_ID_SEEDMAX];
	pid_t    pid;
	uid_t    uid;
};

void od_idmgr_init(od_idmgr_t*);
int  od_idmgr_seed(od_idmgr_t*);
void od_idmgr_generate(od_idmgr_t*, od_id_t*);

static inline int
od_idmgr_cmp(od_id_t *a, od_id_t *b)
{
	return *(uint64_t*)a->id == *(uint64_t*)b->id;
}

#endif /* OD_ID_H */
