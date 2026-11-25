/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <od_memory.h>
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
	od_free(ctx->last_unknown_ident);
	ctx->last_unknown_ident = NULL;
	memset(ctx, 0, sizeof(*ctx));
}

int64_t od_cfg_read_integer(const char *text, char **end)
{
	const char *digits = text + (*text == '+' || *text == '-');
	int base = strncasecmp(digits, "0x", 2) == 0 ? 16 : 10;
	errno = 0;
	return strtoll(text, end, base);
}

const char *od_cfg_parse_size(const char *text, int64_t *out)
{
	char *suffix;
	int64_t n = od_cfg_read_integer(text, &suffix);
	if (suffix == text || isspace((unsigned char)*text)) {
		return "invalid size";
	}
	if (errno == ERANGE) {
		return "size is out of range";
	}
	int64_t scale = 1;
	if (*suffix != 0) {
		switch (tolower((unsigned char)*suffix++)) {
		case 'b':
			break;
		case 'k':
			scale = 1024;
			break;
		case 'm':
			scale = 1024 * 1024;
			break;
		case 'g':
			scale = 1024 * 1024 * 1024;
			break;
		default:
			return "invalid size unit";
		}
		if (*suffix != 0 &&
		    (scale == 1 || strcasecmp(suffix, "b") != 0)) {
			return "invalid size unit";
		}
	}
	if (__builtin_mul_overflow(n, scale, out)) {
		return "size is out of range";
	}
	return NULL;
}
