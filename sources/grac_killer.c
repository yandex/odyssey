#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>
#include <sys/prctl.h>

static inline bool
od_is_server_busy(od_system_server_t *server)
{
	od_router_t *router = server->global->router;
	return od_atomic_u32_of(&router->clients) |
	       od_atomic_u32_of(&router->clients_routing);
}

static inline int
od_system_server_complete_stop(od_system_server_t *server)
{
	/* shutdown */
	int rc;
	rc = machine_shutdown(server->io);

	if (rc == -1)
		return NOT_OK_RESPONSE;
	return OK_RESPONSE;
}

void
od_grac_shutdown_worker(void *arg)
{

	od_system_t *system     = arg;
	od_instance_t *instance = system->global->instance;
	od_log(&instance->logger,
	       "config",
	       NULL,
	       NULL,
	       "stop to accepting new connections");

	od_setproctitlef(&instance->orig_argv_ptr,
	                 "odyssey: version %s stop accepting any connections and "
	                 "working with old transactions",
	                 OD_VERSION_NUMBER);

	od_router_t *router = system->global->router;
	router->closed      = true;

	od_list_t *i;
	od_list_foreach(&router->servers, i)
	{
		od_system_server_t *server;
		server         = od_container_of(i, od_system_server_t, link);
		server->closed = true;
	}

	od_dbg_printf_on_dvl_lvl(1, "servers closed\n", 0);

	//	od_worker_grac_pool_wait(system->global->worker_pool);

	/* wait for all servers to complete old transations */
	od_list_foreach(&router->servers, i)
	{
		od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);

		while (od_is_server_busy(server)) {
			od_dbg_printf_on_dvl_lvl(1, "waiting for %s\n", server->sid.id);
			// this is blocking call
			// we are throttle ourselves
			//            usleep(1000);
			struct timespec ts;
			if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
				ts.tv_sec += 1;
			} else {
				usleep(100);
			}
			//            usleep(100);
			//			sleep(1);
			//			int rc = sem_timedwait(&router->wait_shutdown_cond,
			//&ts);
			int rc = -1;
			if (rc == -1) {
				od_dbg_printf_on_dvl_lvl(
				  1, "condition not satisfied %s\n", server->sid.id);
			} else {
				od_dbg_printf_on_dvl_lvl(
				  1, "condition met %s\n", server->sid.id);
			}
			mm_yield;
			machine_sleep(1000);
		}

		od_dbg_printf_on_dvl_lvl(1, "server shutdown ok %s\n", server->sid.id);
	}

	od_dbg_printf_on_dvl_lvl(1, "shutting down sockets\n", "");

	/* close sockets */
	od_list_foreach(&router->servers, i)
	{
		od_system_server_t *server;
		server = od_container_of(i, od_system_server_t, link);
		od_system_server_complete_stop(server);
	}

	machine_stop(system->machine);
	od_dbg_printf_on_dvl_lvl(
	  1, "waiting done, sending sigint to own process %d\n", instance->pid.pid);

	kill(instance->pid.pid, SIGINT);
}
