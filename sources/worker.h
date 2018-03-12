#ifndef OD_WORKER_H
#define OD_WORKER_H

/*
 * Odyssey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_worker od_worker_t;

struct od_worker
{
	int64_t            machine;
	int                id;
	machine_channel_t *task_channel;
	od_system_t       *system;
};

void od_worker_init(od_worker_t*, od_system_t*, int);
int  od_worker_start(od_worker_t*);

#endif /* OD_WORKER_H */
