#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <types.h>
#include <od_c.h>

typedef struct od_worker od_worker_t;

struct od_worker {
	int64_t machine;
	int id;
	machine_channel_t *task_channel;
	uint64_t clients_processed;
	od_global_t *global;
};

void od_worker_init(od_worker_t *, od_global_t *, int);
int od_worker_start(od_worker_t *);
void od_worker_shutdown(od_worker_t *);

od_linear_alloc_t *od_worker_get_local_linear_alloc(void);

extern OD_THREAD_LOCAL bool worker_shutting_down;
static inline bool od_worker_is_shutting_down(void)
{
	return worker_shutting_down;
}
