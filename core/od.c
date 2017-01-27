
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

#include <machinarium.h>
#include <soprano.h>

#include "od_macro.h"
#include "od_build.h"
#include "od_list.h"
#include "od_pid.h"
#include "od_syslog.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od_server.h"
#include "od_server_pool.h"
#include "od_route_id.h"
#include "od_route.h"
#include "od_client.h"
#include "od_client_pool.h"
#include "od_route_pool.h"
#include "od.h"
#include "od_daemon.h"
#include "od_pooler.h"

void od_init(od_t *od)
{
	od_pidinit(&od->pid);
	od_syslog_init(&od->syslog);
	od_loginit(&od->log, &od->pid, &od->syslog);
	od_schemeinit(&od->scheme);
	od_configinit(&od->config, &od->log, &od->scheme);
}

void od_free(od_t *od)
{
	if (od->scheme.pid_file)
		od_pidfile_unlink(&od->pid, od->scheme.pid_file);
	od_schemefree(&od->scheme);
	od_configclose(&od->config);
	od_logclose(&od->log);
	od_syslog_close(&od->syslog);
}

static inline void
od_usage(od_t *od, char *path)
{
	od_log(&od->log, NULL, "odissey (version: %s)", OD_GIT_VERSION);
	od_log(&od->log, NULL, "usage: %s <config_file>", path);
}

int od_main(od_t *od, int argc, char **argv)
{
	/* validate command line options */
	if (argc != 2) {
		od_usage(od, argv[0]);
		return 1;
	}
	char *config_file;
	if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0 ||
		    strcmp(argv[1], "--help") == 0) {
			od_usage(od, argv[0]);
			return 0;
		}
		config_file = argv[1];
	}
	/* read config file */
	int rc;
	rc = od_configopen(&od->config, config_file);
	if (rc == -1)
		return 1;
	rc = od_configparse(&od->config);
	if (rc == -1)
		return 1;
	/* set log verbosity level */
	od_logset_verbosity(&od->log, od->scheme.log_verbosity);
	/* run as daemon */
	if (od->scheme.daemonize) {
		rc = od_daemonize();
		if (rc == -1)
			return 1;
		/* update pid */
		od_pidinit(&od->pid);
	}
	/* reopen log file after config parsing */
	if (od->scheme.log_file) {
		rc = od_logopen(&od->log, od->scheme.log_file);
		if (rc == -1) {
			od_error(&od->log, NULL, "failed to open log file '%s'",
			         od->scheme.log_file);
			return 1;
		}
	}
	/* syslog */
	if (od->scheme.syslog) {
		od_syslog_open(&od->syslog,
		               od->scheme.syslog_ident,
		               od->scheme.syslog_facility);
	}
	od_log(&od->log, NULL, "odissey (version: %s)", OD_GIT_VERSION);
	od_log(&od->log, NULL, "");
	/* validate configuration scheme */
	rc = od_schemevalidate(&od->scheme, &od->log);
	if (rc == -1)
		return 1;
	/* print configuration scheme */
	if (od->scheme.log_verbosity >= 1) {
		od_schemeprint(&od->scheme, &od->log);
		od_log(&od->log, NULL, "");
	}
	/* create pid file */
	if (od->scheme.pid_file)
		od_pidfile_create(&od->pid, od->scheme.pid_file);

	/* run connection pooler */
	od_pooler_t pooler;
	rc = od_pooler_init(&pooler, od);
	if (rc == -1)
		return 1;
	rc = od_pooler_start(&pooler);
	if (rc == -1)
		return 1;
	return 0;
}
