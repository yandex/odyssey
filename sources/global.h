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
	void *extensions;
	void *hba;

	od_atomic_u64_t pause;
};

static inline void od_global_init(od_global_t *global, void *instance,
				  void *system, void *router, void *cron,
				  void *worker_pool, void *extensions,
				  void *hba)
{
	global->instance = instance;
	global->system = system;
	global->router = router;
	global->cron = cron;
	global->worker_pool = worker_pool;
	global->extensions = extensions;
	global->hba = hba;

	od_atomic_u64_set(&global->pause, 0ULL);
}

static inline uint64_t od_global_is_paused(od_global_t *global)
{
	return od_atomic_u64_of(&global->pause);
}

static inline void od_global_pause(od_global_t *global)
{
	od_atomic_u64_set(&global->pause, 1ULL);
}

static inline void od_global_resume(od_global_t *global)
{
	od_atomic_u64_set(&global->pause, 0ULL);
}

#endif /* ODYSSEY_GLOBAL_H */
