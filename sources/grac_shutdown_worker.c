
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

void od_grac_shutdown_worker(void *arg)
{
	od_system_t *system = arg;
	od_instance_t *instance = system->global->instance;
	od_log(&instance->logger, "config", NULL, NULL,
	       "stop to accepting new connections");

	od_setproctitlef(
		&instance->orig_argv_ptr,
		"odyssey: version %s stop accepting any connections and "
		"working with old transactions",
		OD_VERSION_NUMBER);

	od_router_t *router = system->global->router;

	od_list_t *i;
	od_list_foreach(&router->servers, i)
	{
		od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);
		server->closed = true;
	}

	od_dbg_printf_on_dvl_lvl(1, "servers closed, errors: %d\n", 0);

	/* wait for all servers to complete old transations */
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

	od_dbg_printf_on_dvl_lvl(1, "shutting down sockets %s\n", "");

	/* close sockets */
	od_list_foreach(&router->servers, i)
	{
		od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);
		od_system_server_complete_stop(server);
	}

	machine_stop(system->machine);
	od_dbg_printf_on_dvl_lvl(
		1, "waiting done, sending sigint to own process %d\n",
		instance->pid.pid);

	kill(instance->pid.pid, SIGINT);
}
