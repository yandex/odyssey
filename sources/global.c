/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <types.h>
#include <global.h>
#include <instance.h>

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

	memset(&global->host_watcher, 0, sizeof(global->host_watcher));
	memset(&global->tls_watcher, 0, sizeof(global->tls_watcher));

	return 0;
}

od_global_t *od_global_create(od_instance_t *instance, od_system_t *system,
			      od_router_t *router, od_cron_t *cron,
			      od_worker_pool_t *worker_pool,
			      od_extension_t *extensions, od_hba_t *hba)
{
	od_global_t *g = od_malloc(sizeof(od_global_t));
	if (g == NULL) {
		return NULL;
	}

	if (od_global_init(g, instance, system, router, cron, worker_pool,
			   extensions, hba)) {
		od_free(g);
		return NULL;
	}

	od_global_set(g);

	return g;
}

void od_global_set(od_global_t *global)
{
	current_global = global;
}

od_global_t *od_global_get()
{
	return current_global;
}

od_logger_t *od_global_get_logger()
{
	od_global_t *global = od_global_get();

	return &global->instance->logger;
}

od_instance_t *od_global_get_instance()
{
	od_global_t *global = od_global_get();

	return global->instance;
}

int od_global_is_in_soft_oom(od_global_t *global, uint64_t *used_memory)
{
	od_config_t *config = &global->instance->config;

	if (!config->soft_oom.enabled) {
		return 0;
	}

	return od_soft_oom_is_in_soft_oom(&global->soft_oom, used_memory);
}

void od_global_read_host_utilization(od_global_t *global, float *cpu,
				     float *mem)
{
	od_config_t *config = &global->instance->config;

	if (!config->host_watcher_enabled) {
		*cpu = 0.0;
		*mem = 0.0;
		return;
	}

	od_host_watcher_read(&global->host_watcher, cpu, mem);
}
