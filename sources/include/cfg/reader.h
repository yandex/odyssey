#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <stdbool.h>
#include <cfg/model.h>
#include <cfg/diag.h>
#include <cfg/ctx.h>

#define OD_CFG_AUTOCONF_SUFFIX ".autoconf"

/*
 * Parse a file or directory, then apply autoconf_path on top. With NULL,
 * use <path>.autoconf for a file or <path>/odyssey.autoconf for a directory.
 */
int od_cfg_parse_with_autoconf(const char *path, bool is_dir,
			       const char *autoconf_path, od_cfg_model_t *model,
			       od_cfg_diag_list_t *diags);

int od_cfg_parse_file_with_autoconf(const char *path, const char *autoconf_path,
				    od_cfg_model_t *model,
				    od_cfg_diag_list_t *diags);

/* internal: parse with explicit include depth; used from parse.y include action */
int od_cfg_parse_file_depth(const char *path, od_cfg_model_t *model,
			    od_cfg_diag_list_t *diags, int depth,
			    int allow_include);

int od_cfg_include_dir(od_cfg_parse_ctx_t *ctx, const char *path,
		       od_cfg_location_t location);

/*
 * Parses a single autoconf file the way it is read on top of the main
 * config. Shared by reading and by ALTER SYSTEM, which checks a new file
 * before it replaces the old one.
 */
int od_cfg_parse_autoconf(const char *path, od_cfg_model_t *model,
			  od_cfg_diag_list_t *diags);
