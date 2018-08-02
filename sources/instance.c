
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>

#include <machinarium.h>
#include <shapito.h>

#include "sources/macro.h"
#include "sources/version.h"
#include "sources/atomic.h"
#include "sources/util.h"
#include "sources/error.h"
#include "sources/list.h"
#include "sources/pid.h"
#include "sources/id.h"
#include "sources/logger.h"
#include "sources/daemon.h"
#include "sources/config.h"
#include "sources/config_reader.h"
#include "sources/msg.h"
#include "sources/global.h"
#include "sources/stat.h"
#include "sources/server.h"
#include "sources/server_pool.h"
#include "sources/client.h"
#include "sources/client_pool.h"
#include "sources/route_id.h"
#include "sources/route.h"
#include "sources/route_pool.h"
#include "sources/io.h"
#include "sources/instance.h"
#include "sources/router_cancel.h"
#include "sources/router.h"
#include "sources/console.h"
#include "sources/system.h"
#include "sources/cron.h"
#include "sources/worker.h"
#include "sources/worker_pool.h"
#include "sources/frontend.h"

void od_instance_init(od_instance_t *instance)
{
	od_pid_init(&instance->pid);
	od_logger_init(&instance->logger, &instance->pid);
	od_config_init(&instance->config);
	od_idmgr_init(&instance->id_mgr);
	shapito_cache_init(&instance->stream_cache);
	instance->config_file = NULL;
	instance->is_shared = 0;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGPIPE);
	sigprocmask(SIG_BLOCK, &mask, NULL);
}

void od_instance_free(od_instance_t *instance)
{
	if (instance->config.pid_file)
		od_pid_unlink(&instance->pid, instance->config.pid_file);
	od_config_free(&instance->config);
	od_logger_close(&instance->logger);
	shapito_cache_free(&instance->stream_cache);
	machinarium_free();
}

static inline void
od_usage(od_instance_t *instance, char *path)
{
	od_log(&instance->logger, "init", NULL, NULL,
	       "odyssey (git: %s %s)",
	       OD_VERSION_GIT,
	       OD_VERSION_BUILD);
	od_log(&instance->logger, "init", NULL, NULL,
	       "usage: %s <config_file>", path);
}

int od_instance_main(od_instance_t *instance, int argc, char **argv)
{
	/* validate command line options */
	if (argc != 2) {
		od_usage(instance, argv[0]);
		return -1;
	}
	if (strcmp(argv[1], "-h") == 0 ||
	    strcmp(argv[1], "--help") == 0) {
		od_usage(instance, argv[0]);
		return 0;
	}
	instance->config_file = argv[1];

	/* read config file */
	od_error_t error;
	od_error_init(&error);

	int rc;
	rc = od_configreader_import(&instance->config, &error, instance->config_file);
	if (rc == -1) {
		od_error(&instance->logger, "config", NULL, NULL,
		         "%s", error.error);
		return -1;
	}

	/* validate configuration config */
	rc = od_config_validate(&instance->config, &instance->logger);
	if (rc == -1)
		return -1;

	/* set log format */
	od_logger_set_format(&instance->logger, instance->config.log_format);

	/* set log debug messages */
	od_logger_set_debug(&instance->logger, instance->config.log_debug);

	/* set log to stdout */
	od_logger_set_stdout(&instance->logger, instance->config.log_to_stdout);

	/* set cache limits */
	shapito_cache_set_limit(&instance->stream_cache, instance->config.cache);
	shapito_cache_set_limit_size(&instance->stream_cache, instance->config.cache_chunk);

	/* run as daemon */
	if (instance->config.daemonize) {
		rc = od_daemonize();
		if (rc == -1)
			return -1;
		/* update pid */
		od_pid_init(&instance->pid);
	}

	/* init machinarium machinery */
	machinarium_set_stack_size(instance->config.coroutine_stack_size);
	machinarium_set_pool_size(instance->config.resolvers);
	machinarium_set_coroutine_cache_size(instance->config.cache_coroutine);
	rc = machinarium_init();
	if (rc == -1) {
		od_error(&instance->logger, "init", NULL, NULL,
		         "failed to init machinarium");
		return -1;
	}

	/* reopen log file after config parsing */
	if (instance->config.log_file) {
		rc = od_logger_open(&instance->logger, instance->config.log_file);
		if (rc == -1) {
			od_error(&instance->logger, "init", NULL, NULL,
			         "failed to open log file '%s'",
			         instance->config.log_file);
			return -1;
		}
	}

	/* syslog */
	if (instance->config.log_syslog) {
		od_logger_open_syslog(&instance->logger,
		                      instance->config.log_syslog_ident,
		                      instance->config.log_syslog_facility);
	}
	od_log(&instance->logger, "init", NULL, NULL, "odyssey (git: %s %s)",
	       OD_VERSION_GIT,
	       OD_VERSION_BUILD);
	od_log(&instance->logger, "init", NULL, NULL, "");

	/* print configuration */
	od_log(&instance->logger, "init", NULL, NULL, "using configuration file '%s'",
	       instance->config_file);
	od_log(&instance->logger, "init", NULL, NULL, "");

	if (instance->config.log_config)
		od_config_print(&instance->config, &instance->logger, 0);

	/* create pid file */
	if (instance->config.pid_file)
		od_pid_create(&instance->pid, instance->config.pid_file);

	/* seed id manager */
	od_idmgr_seed(&instance->id_mgr);

	/* is multi-worker deploy */
	instance->is_shared = instance->config.workers > 1;

	/* prepare global services */
	od_system_t system;
	od_system_init(&system);

	od_router_t router;
	od_console_t console;
	od_cron_t cron;
	od_workerpool_t worker_pool;

	od_global_t *global;
	global = &system.global;
	global->instance    = instance;
	global->system      = &system;
	global->router      = &router;
	global->console     = &console;
	global->cron        = &cron;
	global->worker_pool = &worker_pool;

	od_router_init(&router, global);
	od_console_init(&console, global);
	od_cron_init(&cron, global);
	od_workerpool_init(&worker_pool);

	/* start system machine thread */
	rc = od_system_start(&system);
	if (rc == -1)
		return -1;

	machine_wait(system.machine);
	return 0;
}
