#ifndef OD_ID_H
#define OD_ID_H

/*
 * ODISSEY.
 *
 * PostgreSQL connection pooler and request router.
*/

typedef struct od_id    od_id_t;
typedef struct od_idmgr od_idmgr_t;

struct od_id
{
	uint8_t id[8];
};

struct od_idmgr
{
	uint64_t seq;
	uint8_t  seed[8];
	pid_t    pid;
	uid_t    uid;
};

void od_idmgr_init(od_idmgr_t*);
int  od_idmgr_seed(od_idmgr_t*, od_log_t*);
void od_idmgr_generate(od_idmgr_t*, od_id_t*);

#endif /* OD_ID_H */
