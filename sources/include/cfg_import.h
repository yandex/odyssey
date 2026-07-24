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
