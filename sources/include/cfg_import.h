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

int od_cfg_import(od_logger_t *logger, od_config_t *config, od_rules_t *rules,
		  od_global_t *global, od_hba_rules_t *hba_rules,
		  const char *config_file);

/* autoconf_file is read in place of <config_file>.autoconf, NULL is default */
int od_cfg_import_with_autoconf(od_logger_t *logger, od_config_t *config,
				od_rules_t *rules, od_global_t *global,
				od_hba_rules_t *hba_rules,
				const char *config_file,
				const char *autoconf_file);

/*
 * Loads the configuration the way RELOAD does into temporary structures
 * and checks it without applying anything. Errors go to the log. Used by
 * odyssey --test and by ALTER SYSTEM for a candidate autoconf file.
 * Returns 0 if RELOAD would accept the configuration.
 */
int od_cfg_check(od_logger_t *logger, od_global_t *global,
		 const char *config_file, const char *autoconf_file);
