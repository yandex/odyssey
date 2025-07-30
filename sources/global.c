/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

od_global_t *current_global od_read_mostly = NULL;

int od_global_init(od_global_t *global, od_instance_t *instance,
		   od_system_t *system, od_router_t *router, od_cron_t *cron,
		   od_worker_pool_t *worker_pool, od_extension_t *extensions,
		   od_hba_t *hba)
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

	memset(&global->soft_oom, 0, sizeof(global->soft_oom));

	return 0;
}

void od_global_set(od_global_t *global)
{
	current_global = global;
}

od_global_t *od_global_get()
{
	return current_global;
}

int od_global_is_in_soft_oom(od_global_t *global, uint64_t *used_memory)
{
	od_config_t *config = &global->instance->config;

	if (!config->soft_oom.enabled) {
		return 0;
	}

	*used_memory = atomic_load(&global->soft_oom.current_memory_usage);

	return *used_memory >= config->soft_oom.limit_bytes;
}
