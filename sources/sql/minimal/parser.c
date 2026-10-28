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
#include <sql/minimal/keywords.h>
#include <sql/minimal/parser.h>

static inline int is_ident_start(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline int is_ident_cont(char c)
{
	return is_ident_start(c) || (c >= '0' && c <= '9');
}

/*
 * Every statement the grammar accepts starts with a keyword, while most
 * of the real traffic (SELECT, INSERT, UPDATE, ...) does not, and is
 * rejected by bison on the very first token. Setting up flex for that
 * costs more than the whole parse, so peek at the first word here and
 * bail out early if it can not start a statement.
 *
 * Mirrors the INITIAL start condition of scan.l: whitespace, "--" and
 * slash-star comments are skipped, then an IDENT is taken. Anything else
 * (punctuation, non-ASCII, empty input) is left to the real scanner.
 *
 * Returns 1 if the input is guaranteed to be a syntax error.
 */
static int fast_reject(const char *s, size_t len)
{
	size_t i = 0;

	for (;;) {
		while (i < len && (s[i] == ' ' || s[i] == '\t' ||
				   s[i] == '\r' || s[i] == '\n')) {
			i++;
		}

		if (i + 1 < len && s[i] == '-' && s[i + 1] == '-') {
			while (i < len && s[i] != '\n') {
				i++;
			}
			continue;
		}

		if (i + 1 < len && s[i] == '/' && s[i + 1] == '*') {
			i += 2;
			while (i < len && !(s[i] == '*' && i + 1 < len &&
					    s[i + 1] == '/')) {
				i++;
			}
			if (i >= len) {
				/* unterminated, let the scanner report it */
				return 0;
			}
			i += 2;
			continue;
		}

		break;
	}

	if (i >= len || !is_ident_start(s[i])) {
		return 0;
	}

	size_t start = i;
	while (i < len && is_ident_cont(s[i])) {
		i++;
	}

	int token = od_sql_minimal_keyword_lookup(s + start, i - start);
	return !od_sql_minimal_keyword_starts_stmt(token);
}

od_sql_minimal_node_t *od_sql_minimal_parse(const char *input, size_t input_len,
					    od_linear_alloc_t *arena,
					    od_sql_minimal_error_cb_t error_cb,
					    void *userdata)
{
	if (input == NULL) {
		return NULL;
	}

	od_linear_alloc_reset(arena, 0);

	od_sql_minimal_parse_ctx_t ctx;
	od_sql_minimal_parse_ctx_init(&ctx, input, input_len, arena, error_cb,
				      userdata);

	if (fast_reject(ctx.scanbuf, ctx.scanbuflen)) {
		/* same report bison would make with parse.error simple */
		if (error_cb) {
			error_cb("syntax error", userdata);
		}
		return NULL;
	}

	/*
	 * set up longjmp recovery before calling flex, if the scanner
	 * hits OOM (arena full), YY_FATAL_ERROR does longjmp back here
	 * instead of calling exit(1). We treat it as a normal parse error
	 */
	ctx.fatal_set = 1;
	if (mm_setjmp(ctx.fatal_jmp) != 0) {
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

	ctx.scanbuf = buf->yy_ch_buf;
	int rc = od_sql_minimal_yyparse(scanner, &ctx);

	od_sql_minimal_yy_delete_buffer(buf, scanner);
	od_sql_minimal_yylex_destroy(scanner);

	if (rc != 0 || ctx.had_error) {
		od_sql_minimal_node_free(ctx.result);
		return NULL;
	}

	return ctx.result;
}
