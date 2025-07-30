
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

static inline int od_system_server_complete_stop(od_system_server_t *server)
{
	/* shutdown */
	int rc;
	rc = machine_shutdown(server->io);

	if (rc == -1)
		return NOT_OK_RESPONSE;
	return OK_RESPONSE;
}

static inline void od_grac_shutdown_timeout_killer(void *arg)
{
	od_instance_t *instance = arg;

	machine_sleep(instance->config.graceful_shutdown_timeout_ms);

	if (machine_errno() == ECANCELED) {
		return;
	}

	od_error(&instance->logger, "grac-shutdown", NULL, NULL,
		 "graceful shutdown timeout");

	exit(1);
}

void od_grac_shutdown_worker(void *arg)
{
	od_grac_shutdown_worker_arg_t *warg =
		mm_cast(od_grac_shutdown_worker_arg_t *, arg);

	od_worker_pool_t *worker_pool;
	od_system_t *system;
	od_instance_t *instance;
	od_router_t *router;
	machine_channel_t *channel;

	system = warg->system;
	worker_pool = system->global->worker_pool;
	instance = system->global->instance;
	router = system->global->router;
	channel = warg->channel;

	int timeout_killer_id = INVALID_COROUTINE_ID;

	if (instance->config.graceful_shutdown_timeout_ms != 0) {
		timeout_killer_id = machine_coroutine_create_named(
			od_grac_shutdown_timeout_killer, instance,
			"grac_timeout");
		if (timeout_killer_id == -1) {
			od_error(&instance->logger, "grac-shutdown", NULL, NULL,
				 "can't create timeout killer coroutine");
			exit(1);
		}
	}

	od_log(&instance->logger, "config", NULL, NULL,
	       "stop to accepting new connections");

	od_setproctitlef(
		&instance->orig_argv_ptr,
		"odyssey: version %s stop accepting any connections and "
		"working with old transactions",
		OD_VERSION_NUMBER);

	od_list_t *i;
	od_list_foreach(&router->servers, i)
	{
		od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);
		atomic_store(&server->closed, true);
	}

	od_dbg_printf_on_dvl_lvl(1, "servers closed, errors: %d\n", 0);

	/* wait for all servers to complete old transactions */
	od_list_foreach(&router->servers, i)
	{
#if OD_DEVEL_LVL != OD_RELEASE_MODE
		od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);
#else
		od_attribute_unused() od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);
#endif

		while (od_atomic_u32_of(&router->clients_routing) ||
		       od_atomic_u32_of(&router->clients)) {
			od_dbg_printf_on_dvl_lvl(1, "waiting for %s\n",
						 server->sid.id);
			machine_sleep(1000);
		}

		od_dbg_printf_on_dvl_lvl(1, "server shutdown ok %s\n",
					 server->sid.id);
	}

	/* let storage watchdog's finish */
	od_rules_cleanup(&system->global->router->rules);

	od_worker_pool_shutdown(worker_pool);
	od_worker_pool_wait_gracefully_shutdown(worker_pool);

	od_dbg_printf_on_dvl_lvl(1, "shutting down sockets %s\n", "");

	/* close sockets */
	od_list_foreach(&router->servers, i)
	{
		od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);
		od_system_server_complete_stop(server);
	}

	// lock here
	od_cron_stop_and_wait(system->global->cron);

	od_extension_free(&instance->logger, system->global->extensions);

#ifdef OD_SYSTEM_SHUTDOWN_CLEANUP
	od_router_free(system->global->router);

	od_system_cleanup(system);

	/* stop machinaruim and free */
	od_instance_free(instance);
#endif

	// (WIP) it's not useful yet
	if (timeout_killer_id != INVALID_COROUTINE_ID) {
		int rc = machine_cancel(timeout_killer_id);
		if (rc != 0) {
			od_fatal(&instance->logger, "system", NULL, NULL,
				 "failed to cancel timeout killer");
		}
	}

	// (WIP) it's not useful yet
	machine_msg_t *msg = machine_msg_create(0);
	if (msg == NULL) {
		od_fatal(&instance->logger, "system", NULL, NULL,
			 "failed to create a message in sigwaiter");
	}

	machine_msg_set_type(msg, OD_MSG_GRAC_SHUTDOWN_FINISHED);
	int rc = machine_channel_write(channel, msg);
	if (rc != 0) {
		od_fatal(&instance->logger, "system", NULL, NULL,
			 "failed to write a message in sigwaiter");
	}
}
