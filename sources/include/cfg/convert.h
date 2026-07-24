#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <cfg/model.h>

#include <types.h>
#include <hba.h>
#include <config.h>

int od_cfg_convert_model(const od_cfg_model_t *model, od_config_t *config,
			 od_rules_t *rules, od_global_t *global,
			 od_hba_rules_t *hba_rules, od_cfg_diag_list_t *diags);
