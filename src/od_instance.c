
/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_version.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_daemon.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_instance.h"
#include "od_pooler.h"

void od_instance_init(od_instance_t *instance)
{
	od_pid_init(&instance->pid);
	od_syslog_init(&instance->syslog);
	od_log_init(&instance->log, &instance->pid, &instance->syslog);
	od_scheme_init(&instance->scheme);
	od_config_init(&instance->config, &instance->log, &instance->scheme);

	signal(SIGPIPE, SIG_IGN);
}

void od_instance_free(od_instance_t *instance)
{
	if (instance->scheme.pid_file)
		od_pid_unlink(&instance->pid, instance->scheme.pid_file);
	od_scheme_free(&instance->scheme);
	od_config_close(&instance->config);
	od_log_close(&instance->log);
	od_syslog_close(&instance->syslog);
}

static inline void
od_usage(od_instance_t *instance, char *path)
{
	od_log(&instance->log, NULL, "odissey (version: %s %s)",
	       OD_VERSION_GIT,
	       OD_VERSION_BUILD);
	od_log(&instance->log, NULL, "usage: %s <config_file>", path);
}

int od_instance_main(od_instance_t *instance, int argc, char **argv)
{
	/* validate command line options */
	if (argc != 2) {
		od_usage(instance, argv[0]);
		return 1;
	}
	char *config_file;
	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0 ||
		    strcmp(argv[1], "--help") == 0) {
			od_usage(instance, argv[0]);
			return 0;
		}
		config_file = argv[1];
	}
	/* read config file */
	int rc;
	rc = od_config_open(&instance->config, config_file);
	if (rc == -1)
		return 1;
	rc = od_config_parse(&instance->config);
	if (rc == -1)
		return 1;
	/* set log verbosity level */
	od_logset_verbosity(&instance->log, instance->scheme.log_verbosity);
	/* run as daemon */
	if (instance->scheme.daemonize) {
		rc = od_daemonize();
		if (rc == -1)
			return 1;
		/* update pid */
		od_pid_init(&instance->pid);
	}
	/* reopen log file after config parsing */
	if (instance->scheme.log_file) {
		rc = od_log_open(&instance->log, instance->scheme.log_file);
		if (rc == -1) {
			od_error(&instance->log, NULL, "failed to open log file '%s'",
			         instance->scheme.log_file);
			return 1;
		}
	}
	/* syslog */
	if (instance->scheme.syslog) {
		od_syslog_open(&instance->syslog,
		               instance->scheme.syslog_ident,
		               instance->scheme.syslog_facility);
	}
	od_log(&instance->log, NULL, "odissey (version: %s %s)",
	       OD_VERSION_GIT,
	       OD_VERSION_BUILD);
	od_log(&instance->log, NULL, "");
	/* validate configuration scheme */
	rc = od_scheme_validate(&instance->scheme, &instance->log);
	if (rc == -1)
		return 1;
	/* print configuration scheme */
	if (instance->scheme.log_verbosity >= 1) {
		od_scheme_print(&instance->scheme, &instance->log);
		od_log(&instance->log, NULL, "");
	}
	/* create pid file */
	if (instance->scheme.pid_file)
		od_pid_create(&instance->pid, instance->scheme.pid_file);
	/* run connection pooler */
	od_pooler_t pooler;
	od_pooler_init(&pooler, instance);
	rc = od_pooler_start(&pooler);
	if (rc == -1)
		return 1;
	return 0;
}
