/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <od_memory.h>
#include <cfg/ctx.h>

void od_cfg_parse_ctx_init(od_cfg_parse_ctx_t *ctx, const char *filename,
			   od_cfg_model_t *model, od_cfg_diag_list_t *diags)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->filename = filename;
	ctx->model = model;
	ctx->diags = diags;

	ctx->lexer_line = 1;
	ctx->lexer_column = 1;
	ctx->lexer_offset = 0;
}

void od_cfg_parse_ctx_free(od_cfg_parse_ctx_t *ctx)
{
	od_free(ctx->last_unknown_ident);
	ctx->last_unknown_ident = NULL;

	for (size_t i = 0; i < ctx->owned_count; i++) {
		od_free(ctx->owned_paths[i]);
	}
	od_free(ctx->owned_paths);

	memset(ctx, 0, sizeof(*ctx));
}
