#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <logger.h>

/*
 * get parent odyssey pid if current odyssey was started during online restart
 * -1 otherwise
 *
 * should be called only on start of odyssey, because it uses plain envp
 * (which may be changed by setproctitle)
 */
pid_t od_restart_get_ppid(void);

pid_t od_restart_run_new_binary(void);

void od_restart_terminate_parent(void);

DEFINE_SIMPLE_MODULE_LOGGER(online_restart, "online-restart")
