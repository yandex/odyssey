/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <cfg/ctx.h>

void od_cfg_parse_ctx_init(od_cfg_parse_ctx_t *ctx, const char *filename,
			   char *input, size_t input_size,
			   od_cfg_model_t *model, od_cfg_diag_list_t *diags)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->filename = filename;
	ctx->input = input;
	ctx->input_size = input_size;
	ctx->model = model;
	ctx->diags = diags;

	ctx->lexer_line = 1;
	ctx->lexer_column = 1;
	ctx->lexer_offset = 0;
}

void od_cfg_parse_ctx_free(od_cfg_parse_ctx_t *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}
