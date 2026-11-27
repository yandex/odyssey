#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <config.h>
#include <logger.h>
#include <hba.h>
#include <rules.h>
#include <global.h>

/* Pass an autoconf file to check a candidate ALTER SYSTEM change. */
int od_cfg_import(od_instance_t *instance, od_config_t *config,
		  od_rules_t *rules, od_global_t *global,
		  od_hba_rules_t *hba_rules, const char *autoconf_file);

/*
 * Loads the configuration the way RELOAD does into temporary structures
 * and checks it without accessing running services. Errors go to the log. Used by
 * odyssey --test and by ALTER SYSTEM for a candidate autoconf file.
 * Returns 0 if RELOAD would accept the configuration.
 */
int od_cfg_check(od_instance_t *instance, const char *autoconf_file);
