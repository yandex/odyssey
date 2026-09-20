#include <odyssey.h>
#include <sql/full/ctx.h>

void od_sql_full_parse_ctx_init(od_sql_full_parse_ctx_t *ctx, const char *input,
				size_t input_len,
				od_sql_full_error_cb_t error_cb, void *userdata)
{
	(void)input;
	(void)input_len;
	memset(ctx, 0, sizeof(*ctx));
	ctx->error_cb = error_cb;
	ctx->error_cb_userdata = userdata;
}
