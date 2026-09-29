#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <cfg/model.h>
#include <cfg/diag.h>
#include <cfg/ctx.h>

int od_cfg_parse_file(const char *path, od_cfg_model_t *model,
		      od_cfg_diag_list_t *diags);

/* internal: parse with explicit include depth; used for .autoconf */
int od_cfg_parse_file_depth(const char *path, od_cfg_model_t *model,
			    od_cfg_diag_list_t *diags, int depth,
			    int allow_include);

int od_cfg_include_push(od_cfg_parse_ctx_t *ctx, void *scanner, char *path);
int od_cfg_include_pop(od_cfg_parse_ctx_t *ctx, void *scanner);
