#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <types.h>
#include <soft_oom.h>
#include <atomic.h>
#include <host_watcher.h>
#include <logger.h>
#include <od_memory.h>

struct od_global {
	od_instance_t *instance;
	od_system_t *system;
	od_router_t *router;
	od_cron_t *cron;
	od_worker_pool_t *worker_pool;
	od_extension_t *extensions;
	od_hba_t *hba;

	od_soft_oom_checker_t soft_oom;

	od_host_watcher_t host_watcher;

	od_atomic_u64_t pause;
	machine_wait_list_t *resume_waiters;
};

od_global_t *od_global_create(od_instance_t *instance, od_system_t *system,
			      od_router_t *router, od_cron_t *cron,
			      od_worker_pool_t *worker_pool,
			      od_extension_t *extensions, od_hba_t *hba);

int od_global_init(od_global_t *global, od_instance_t *instance,
		   od_system_t *system, od_router_t *router, od_cron_t *cron,
		   od_worker_pool_t *worker_pool, od_extension_t *extensions,
		   od_hba_t *hba);

void od_global_set(od_global_t *global);

od_global_t *od_global_get();

od_logger_t *od_global_get_logger();

od_instance_t *od_global_get_instance();

static inline void od_global_destroy(od_global_t *global)
{
	machine_wait_list_destroy(global->resume_waiters);
	od_free(global);
	od_global_set(NULL);
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

int od_global_is_in_soft_oom(od_global_t *global, uint64_t *used_memory);

void od_global_read_host_utilization(od_global_t *global, float *cpu,
				     float *mem);
