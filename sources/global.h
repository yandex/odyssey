#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

struct od_global {
	od_instance_t *instance;
	od_system_t *system;
	od_router_t *router;
	od_cron_t *cron;
	od_worker_pool_t *worker_pool;
	od_extension_t *extensions;
	od_hba_t *hba;

	od_atomic_u64_t pause;
	machine_wait_list_t *resume_waiters;
};

static inline int od_global_init(od_global_t *global, od_instance_t *instance,
				 od_system_t *system, od_router_t *router,
				 od_cron_t *cron, od_worker_pool_t *worker_pool,
				 od_extension_t *extensions, od_hba_t *hba)
{
	global->instance = instance;
	global->system = system;
	global->router = router;
	global->cron = cron;
	global->worker_pool = worker_pool;
	global->extensions = extensions;
	global->hba = hba;

	od_atomic_u64_set(&global->pause, 0ULL);
	global->resume_waiters = machine_wait_list_create(NULL);
	if (global->resume_waiters == NULL) {
		return 1;
	}

	return 0;
}

void od_global_set(od_global_t *global);

od_global_t *od_global_get();

static inline void od_global_destroy(od_global_t *global)
{
	machine_wait_list_destroy(global->resume_waiters);
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
	machine_wait_list_notify_all(global->resume_waiters);
}

static inline int od_global_wait_resumed(od_global_t *global, uint32_t timeout)
{
	if (!od_global_is_paused(global)) {
		return 0;
	}

	int rc = machine_wait_list_wait(global->resume_waiters, timeout);
	if (rc == 0) {
		return 0;
	}
	return 1;
}
