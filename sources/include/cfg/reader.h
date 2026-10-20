#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <cfg/model.h>
#include <cfg/diag.h>

/* <config>.autoconf is read on top of the main config, see ALTER SYSTEM */
#define OD_CFG_AUTOCONF_SUFFIX ".autoconf"

int od_cfg_parse_file(const char *path, od_cfg_model_t *model,
		      od_cfg_diag_list_t *diags);

/*
 * Same as od_cfg_parse_file() with autoconf_path read in place of
 * <path>.autoconf, NULL means the default. ALTER SYSTEM checks a new
 * autoconf file this way before it replaces the old one.
 */
int od_cfg_parse_file_with_autoconf(const char *path, const char *autoconf_path,
				    od_cfg_model_t *model,
				    od_cfg_diag_list_t *diags);

/* internal: parse with explicit include depth; used from parse.y include action */
int od_cfg_parse_file_depth(const char *path, od_cfg_model_t *model,
			    od_cfg_diag_list_t *diags, int depth,
			    int allow_include);

/*
 * Parses a single autoconf file the way it is read on top of the main
 * config. Shared by reading and by ALTER SYSTEM, which checks a new file
 * before it replaces the old one.
 */
int od_cfg_parse_autoconf(const char *path, od_cfg_model_t *model,
			  od_cfg_diag_list_t *diags);
