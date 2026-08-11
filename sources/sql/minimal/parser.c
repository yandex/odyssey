/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * Minimal SQL parser entry point.
 */

#include <odyssey.h>

#include <sql/minimal/scan.h>
#include <sql/minimal/ast.h>
#include <sql/minimal/ctx.h>
#include <sql/minimal/parser.h>

od_sql_minimal_node_t *od_sql_minimal_parse(const char *input, size_t input_len,
					    od_linear_alloc_t *arena,
					    od_sql_minimal_error_cb_t error_cb,
					    void *userdata)
{
	if (input == NULL) {
		return NULL;
	}

	od_sql_minimal_parse_ctx_t ctx;
	od_sql_minimal_parse_ctx_init(&ctx, input, input_len, arena, error_cb,
				      userdata);

	/*
	 * set up longjmp recovery before calling flex, if the scanner
	 * hits OOM (arena full), YY_FATAL_ERROR does longjmp back here
	 * instead of calling exit(1). We treat it as a normal parse error
	 */
	ctx.fatal_set = 1;
	if (setjmp(ctx.fatal_jmp) != 0) {
		/*
		 * scanner hit a fatal error (most likely OOM in arena)
		 * return NULL as a normal parse failure
		 */
		return NULL;
	}

	yyscan_t scanner;
	if (od_sql_minimal_yylex_init_extra(&ctx, &scanner) != 0) {
		if (error_cb) {
			error_cb("failed to initialize scanner", userdata);
		}
		return NULL;
	}

	YY_BUFFER_STATE buf = od_sql_minimal_yy_scan_bytes(
		ctx.scanbuf, (int)ctx.scanbuflen, scanner);
	if (buf == NULL) {
		if (error_cb) {
			error_cb("failed to create scanner buffer", userdata);
		}
		od_sql_minimal_yylex_destroy(scanner);
		return NULL;
	}

	int rc = od_sql_minimal_yyparse(scanner, &ctx);

	od_sql_minimal_yy_delete_buffer(buf, scanner);
	od_sql_minimal_yylex_destroy(scanner);

	if (rc != 0 || ctx.had_error) {
		od_sql_minimal_node_free(ctx.result);
		return NULL;
	}

	return ctx.result;
}
