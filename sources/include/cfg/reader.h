#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <cfg/model.h>
#include <cfg/diag.h>

int od_cfg_parse_file(const char *path, od_cfg_model_t *model,
		      od_cfg_diag_list_t *diags);

/* internal: parse with explicit include depth; used from parse.y include action */
int od_cfg_parse_file_depth(const char *path, od_cfg_model_t *model,
			    od_cfg_diag_list_t *diags, int depth);
