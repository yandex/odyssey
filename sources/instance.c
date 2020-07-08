
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
#include <ctype.h>
#include <inttypes.h>
#include <assert.h>

#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <machinarium.h>
#include <kiwi.h>
#include <odyssey.h>
#include "module.h"

void
od_instance_init(od_instance_t *instance)
{
	od_pid_init(&instance->pid);
	od_logger_init(&instance->logger, &instance->pid);
	od_config_init(&instance->config);
	instance->config_file = NULL;

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGPIPE);
	sigprocmask(SIG_BLOCK, &mask, NULL);
}

void
od_instance_free(od_instance_t *instance)
{
	if (instance->config.pid_file)
		od_pid_unlink(&instance->pid, instance->config.pid_file);
	od_config_free(&instance->config);
	od_log(&instance->logger, "shutdown", NULL, NULL, "Stopping Odyssey");
	od_logger_close(&instance->logger);
	machinarium_free();
}

static inline void
od_usage(od_instance_t *instance, char *path)
{
	od_log(&instance->logger,
	       "init",
	       NULL,
	       NULL,
	       "odyssey (git: %s %s)",
	       OD_VERSION_GIT,
	       OD_VERSION_BUILD);
	od_log(
	  &instance->logger, "init", NULL, NULL, "usage: %s <config_file>", path);
}

int
od_instance_main(od_instance_t *instance, int argc, char **argv)
{
	/* prepare system services */
	od_system_t system;
	od_router_t router;
	od_cron_t cron;
	od_worker_pool_t worker_pool;
	od_module_t modules;
	od_global_t global;

	od_log(&instance->logger, "startup", NULL, NULL, "Starting Odyssey");

	od_system_init(&system);
	od_router_init(&router);
	od_cron_init(&cron);
	od_worker_pool_init(&worker_pool);
	od_modules_init(&modules);
	od_global_init(
	  &global, instance, &system, &router, &cron, &worker_pool, &modules);

	/* validate command line options */
	if (argc != 2) {
		od_usage(instance, argv[0]);
		goto error;
	}
	if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		od_usage(instance, argv[0]);
		od_router_free(&router);
		return 0;
	}
	instance->config_file = argv[1];

	/* read config file */
	od_error_t error;
	od_error_init(&error);
	int rc;
	rc = od_config_reader_import(&instance->config,
	                             &router.rules,
	                             &error,
	                             &modules,
	                             instance->config_file);
	if (rc == -1) {
		od_error(&instance->logger, "config", NULL, NULL, "%s", error.error);
		goto error;
	}

	/* validate configuration */
	rc = od_config_validate(&instance->config, &instance->logger);
	if (rc == -1) {
		goto error;
	}

	/* validate rules */
	rc = od_rules_validate(&router.rules, &instance->config, &instance->logger);
	if (rc == -1) {
		goto error;
	}

	/* configure logger */
	od_logger_set_format(&instance->logger, instance->config.log_format);
	od_logger_set_debug(&instance->logger, instance->config.log_debug);
	od_logger_set_stdout(&instance->logger, instance->config.log_to_stdout);

	/* run as daemon */
	if (instance->config.daemonize) {
		rc = od_daemonize();
		if (rc == -1) {
			goto error;
		}
		/* update pid */
		od_pid_init(&instance->pid);
	}

	/* reopen log file after config parsing */
	if (instance->config.log_file) {
		rc = od_logger_open(&instance->logger, instance->config.log_file);
		if (rc == -1) {
			od_error(&instance->logger,
			         "init",
			         NULL,
			         NULL,
			         "failed to open log file '%s'",
			         instance->config.log_file);
			goto error;
		}
	}

	/* syslog */
	if (instance->config.log_syslog) {
		od_logger_open_syslog(&instance->logger,
		                      instance->config.log_syslog_ident,
		                      instance->config.log_syslog_facility);
	}
	od_log(&instance->logger,
	       "init",
	       NULL,
	       NULL,
	       "odyssey (git: %s %s)",
	       OD_VERSION_GIT,
	       OD_VERSION_BUILD);
	od_log(&instance->logger, "init", NULL, NULL, "");

	/* print configuration */
	od_log(&instance->logger,
	       "init",
	       NULL,
	       NULL,
	       "using configuration file '%s'",
	       instance->config_file);
	od_log(&instance->logger, "init", NULL, NULL, "");

	if (instance->config.log_config) {
		od_config_print(&instance->config, &instance->logger);
		od_rules_print(&router.rules, &instance->logger);
	}

	/* set process priority */
	if (instance->config.priority != 0) {
		int rc;
		rc = setpriority(PRIO_PROCESS, 0, instance->config.priority);
		if (rc == -1) {
			od_error(&instance->logger,
			         "init",
			         NULL,
			         NULL,
			         "failed to set process priority: %s",
			         strerror(errno));
		}
	}

	/* initialize machinarium */
	machinarium_set_stack_size(instance->config.coroutine_stack_size);
	machinarium_set_pool_size(instance->config.resolvers);
	machinarium_set_coroutine_cache_size(instance->config.cache_coroutine);
	machinarium_set_msg_cache_gc_size(instance->config.cache_msg_gc_size);
	rc = machinarium_init();
	if (rc == -1) {
		od_error(
		  &instance->logger, "init", NULL, NULL, "failed to init machinarium");
		goto error;
	}

	/* create pid file */
	if (instance->config.pid_file)
		od_pid_create(&instance->pid, instance->config.pid_file);

	/* start system machine thread */
	rc = od_system_start(&system, &global);
	if (rc == -1) {
		goto error;
	}

	return machine_wait(system.machine);

error:
	od_router_free(&router);
	return NOT_OK_RESPONSE;
}
