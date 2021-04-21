#ifndef ODYSSEY_GLOBAL_H
#define ODYSSEY_GLOBAL_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

typedef struct od_global od_global_t;

struct od_global {
	void *instance;
	void *system;
	void *router;
	void *cron;
	void *worker_pool;
	void *extentions;
};

static inline void od_global_init(od_global_t *global, void *instance,
				  void *system, void *router, void *cron,
				  void *worker_pool, void *extentions)
{
	global->instance = instance;
	global->system = system;
	global->router = router;
	global->cron = cron;
	global->worker_pool = worker_pool;
	global->extentions = extentions;
}

#endif /* ODYSSEY_GLOBAL_H */
