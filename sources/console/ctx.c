/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Console command parser context.
 */

#include <odyssey.h>
#include <console/ctx.h>

void od_console_parse_ctx_init(od_console_parse_ctx_t *ctx, const char *input,
			       size_t input_len, od_linear_alloc_t *arena,
			       od_console_error_cb_t error_cb, void *userdata)
{
	memset(ctx, 0, sizeof(*ctx));

	if (input_len == 0) {
		input_len = strlen(input);
	}

	ctx->scanbuf = input;
	ctx->scanbuflen = input_len;
	ctx->arena = arena;
	ctx->error_cb = error_cb;
	ctx->error_cb_userdata = userdata;
}
