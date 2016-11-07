
/*
 * odissey - PostgreSQL connection pooler and
 *           request router.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "od_macro.h"
#include "od_list.h"
#include "od_log.h"
#include "od_scheme.h"
#include "od_lex.h"
#include "od_config.h"
#include "od.h"

void od_init(od_t *od)
{
	od_loginit(&od->log);
	od_schemeinit(&od->scheme);
	od_configinit(&od->config, &od->log, &od->scheme);
}

void od_free(od_t *od)
{
	od_schemefree(&od->scheme);
	od_configclose(&od->config);
	od_logclose(&od->log);
}

static inline void
od_usage(od_t *od, char *path)
{
	od_log(&od->log, "odissey.");
	od_log(&od->log, "usage: %s <config_file>", path);
}

int od_main(od_t *od, int argc, char **argv)
{
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
		if (strcmp(argv[1], "-v") == 0 ||
		    strcmp(argv[1], "--version") == 0) {
			return 0;
		}
		config_file = argv[1];
	}
	int rc;
	rc = od_configopen(&od->config, config_file);
	if (rc == -1)
		return 1;
	rc = od_configparse(&od->config);
	if (rc == -1)
		return 1;
	od_log(&od->log, "odissey.");
	od_log(&od->log, "");
	od_schemeprint(&od->scheme, &od->log);
	if (od->scheme.log_file) {
		rc = od_logopen(&od->log, od->scheme.log_file);
		if (rc == -1)
			return 1;
	}
	rc = od_schemevalidate(&od->scheme, &od->log);
	if (rc == -1)
		return 1;
	od_log(&od->log, "");
	od_log(&od->log, "ready.");
	return 0;
}
