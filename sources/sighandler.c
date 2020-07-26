

#include "sighandler.h"

static inline od_retcode_t
od_system_gracefully_killer_invoke(od_system_t *system)
{
	od_instance_t *instance = system->global->instance;
	if (instance->shutdowner_id != -1) {
		return OK_RESPONSE;
	}
	int64_t mid;
	mid = machine_create("shutdowner", od_grac_shutdown_worker, system);
	if (mid == -1) {
		od_error(&instance->logger,
		         "gracefully_killer",
		         NULL,
		         NULL,
		         "failed to invoke gracefully killer coroutine");
		return NOT_OK_RESPONSE;
	}

	return OK_RESPONSE;
}

static inline void
od_system_cleanup(od_system_t *system)
{
	od_instance_t *instance = system->global->instance;

	od_list_t *i;
	od_list_foreach(&instance->config.listen, i)
	{
		od_config_listen_t *listen;
		listen = od_container_of(i, od_config_listen_t, link);
		if (listen->host)
			continue;
		/* remove unix socket files */
		char path[PATH_MAX];
		od_snprintf(path,
		            sizeof(path),
		            "%s/.s.PGSQL.%d",
		            instance->config.unix_socket_dir,
		            listen->port);
		unlink(path);
	}
}

void
od_system_signal_handler(void *arg)
{
	od_system_t *system     = arg;
	od_instance_t *instance = system->global->instance;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, OD_SIG_GRACEFUL_SHUTDOWN);

	sigset_t ignore_mask;
	sigemptyset(&ignore_mask);
	sigaddset(&mask, SIGPIPE);
	int rc;
	rc = machine_signal_init(&mask, &ignore_mask);
	if (rc == -1) {
		od_error(&instance->logger,
		         "system",
		         NULL,
		         NULL,
		         "failed to init signal handler");
		return;
	}
	for (;;) {
		rc = machine_signal_wait(UINT32_MAX);
		if (rc == -1)
			break;
		switch (rc) {
			case SIGTERM:
				od_log(&instance->logger,
				       "system",
				       NULL,
				       NULL,
				       "SIGTERM received, shutting down");
				od_worker_pool_stop(system->global->worker_pool);
				/* No time for caution */
				od_system_cleanup(system);
				/* TODO:  */
				od_modules_unload_fast(system->global->modules);
				kill(instance->watchdog_pid.pid, SIGKILL);
				exit(0);
				break;
			case SIGINT:
				od_log(&instance->logger,
				       "system",
				       NULL,
				       NULL,
				       "SIGINT received, shutting down");
				od_worker_pool_stop(system->global->worker_pool);
				/* Prevent OpenSSL usage during deinitialization */
				od_worker_pool_wait(system->global->worker_pool);
				od_modules_unload(&instance->logger, system->global->modules);
				od_system_cleanup(system);
				kill(instance->watchdog_pid.pid, SIGKILL);
				exit(0);
				break;
			case SIGHUP:
				od_log(
				  &instance->logger, "system", NULL, NULL, "SIGHUP received");
				od_system_config_reload(system);
				break;
			case OD_SIG_GRACEFUL_SHUTDOWN:
				if (instance->config.enable_online_restart_feature ||
				    instance->config.graceful_die_on_errors) {
					od_log(&instance->logger,
					       "system",
					       NULL,
					       NULL,
					       "SIG_GRACEFUL_SHUTDOWN received");
					od_system_gracefully_killer_invoke(system);
				} else {
					od_log(&instance->logger,
					       "system",
					       NULL,
					       NULL,
					       "SIGUSR2 received, but online restart feature not "
					       "enabled, doing nothing");
				}
				break;
		}
	}
}
