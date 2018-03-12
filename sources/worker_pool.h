#ifndef OD_WORKER_POOL_H
#define OD_WORKER_POOL_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_workerpool od_workerpool_t;

struct od_workerpool
{
	od_worker_t *pool;
	int          round_robin;
	int          count;
};

void od_workerpool_init(od_workerpool_t*);
int  od_workerpool_start(od_workerpool_t*, od_system_t*, int);
void od_workerpool_feed(od_workerpool_t*, machine_msg_t*);

#endif /* OD_WORKER_POOL_H */
