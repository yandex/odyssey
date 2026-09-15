/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Console command parser entry point.
 */

#include <odyssey.h>

#include <console/scan.h>
#include <console/ast.h>
#include <console/ctx.h>
#include <console/parser.h>

od_console_node_t *od_console_parse(const char *input, size_t input_len,
				    od_linear_alloc_t *arena,
				    od_console_error_cb_t error_cb,
				    void *userdata)
{
	if (input == NULL) {
		return NULL;
	}

	od_linear_alloc_reset(arena);

	od_console_parse_ctx_t ctx;
	od_console_parse_ctx_init(&ctx, input, input_len, arena, error_cb,
				  userdata);

	ctx.fatal_set = 1;
	if (setjmp(ctx.fatal_jmp) != 0) {
		return NULL;
	}

	yyscan_t scanner;
	if (od_console_yylex_init_extra(&ctx, &scanner) != 0) {
		if (error_cb) {
			error_cb("failed to initialize scanner", userdata);
		}
		return NULL;
	}

	YY_BUFFER_STATE buf = od_console_yy_scan_bytes(
		ctx.scanbuf, (int)ctx.scanbuflen, scanner);
	if (buf == NULL) {
		if (error_cb) {
			error_cb("failed to create scanner buffer", userdata);
		}
		od_console_yylex_destroy(scanner);
		return NULL;
	}

	int rc = od_console_yyparse(scanner, &ctx);

	od_console_yy_delete_buffer(buf, scanner);
	od_console_yylex_destroy(scanner);

	if (rc != 0 || ctx.had_error) {
		od_console_node_free(ctx.result);
		return NULL;
	}

	return ctx.result;
}
