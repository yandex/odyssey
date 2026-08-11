%option reentrant
%option bison-bridge
%option bison-locations
%option noyywrap
%option nodefault
%option warn
%option never-interactive
%option noyyalloc
%option noyyrealloc
%option noyyfree
%option prefix="od_sql_minimal_yy"
%option extra-type="od_sql_minimal_parse_ctx_t *"

%top{
/* flex-generated code triggers sign-compare warnings */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <odyssey.h>
#include <od_memory.h>
#include <sql/minimal/parse.tab.h>
#include <sql/minimal/ctx.h>
#include <sql/minimal/keywords.h>

#define PG_NAMEDATALEN 64

/*
 * override flex's YY_FATAL_ERROR so that OOM in the scanner does not
 * call exit(1) — instead longjmp back to od_sql_minimal_parse(), which
 * returns NULL as a normal parse error.
 */
#define YY_FATAL_ERROR(msg) do { \
	od_sql_minimal_parse_ctx_t *_ctx = od_sql_minimal_yyget_extra(yyscanner); \
	if (_ctx && _ctx->fatal_set) { \
		_ctx->had_error = 1; \
		if (_ctx->error_cb) { \
			_ctx->error_cb(msg, _ctx->error_cb_userdata); \
		} \
		longjmp(_ctx->fatal_jmp, 1); \
	} \
	fprintf(stderr, "%s\n", msg); \
	od_assert(0); \
} while (0)
}

%{

#define SET_YYLLOC() \
    (*yylloc = (int)(yytext - yyextra->scanbuf))

void *od_sql_minimal_yyalloc(yy_size_t bytes, yyscan_t yyscanner);
void *od_sql_minimal_yyrealloc(void *ptr, yy_size_t bytes, yyscan_t yyscanner);
void  od_sql_minimal_yyfree(void *ptr, yyscan_t yyscanner);

%}

DIGIT       [0-9]
IDENT_START [A-Za-z_]
IDENT_CONT  [A-Za-z0-9_]
IDENT       {IDENT_START}{IDENT_CONT}*
INTEGER     -?{DIGIT}+

%%

[ \t\r\n]+   { SET_YYLLOC(); /* skip */ }
"--".*       { SET_YYLLOC(); /* skip line comment */ }
"/*"         { SET_YYLLOC();
               int c;
               while ((c = input(yyscanner)) != 0) {
                   if (c == '*') {
                       int next = input(yyscanner);
                       if (next == 0) break;
                       if (next == '/') goto comment_done;
                       unput(next);
                   }
               }
               comment_done:;
               /* skip block comment */
             }

";"          { SET_YYLLOC(); return ';'; }
"="          { SET_YYLLOC(); return '='; }
","          { SET_YYLLOC(); return ','; }
"("          { SET_YYLLOC(); return '('; }
")"          { SET_YYLLOC(); return ')'; }
"."          { SET_YYLLOC(); return '.'; }

{INTEGER} {
	SET_YYLLOC();
	errno = 0;
	int64_t v = strtoll(yytext, NULL, 10);
	if (errno == ERANGE) {
		return ERROR_TOKEN;
	}
	yylval->ival = v;
	return INTEGER;
}

\'([^\'\\]|\\.)*\' {
	SET_YYLLOC();

	size_t len = (size_t)yyleng - 2;
	od_linear_alloc_t *_arena = yyextra->arena;
	char *s = od_linear_alloc_alloc(_arena, len + 1);
	if (s == NULL) {
		return ERROR_TOKEN;
	}
	memcpy(s, yytext + 1, len);
	s[len] = '\0';
	yylval->str = s;

	return SCONST;
}

\"([^\"\\]|\\.)*\" {
	SET_YYLLOC();

	/* double-quoted identifier -- strip quotes, preserve case */
	size_t len = (size_t)yyleng - 2;

	if (len > PG_NAMEDATALEN - 1) {
		return ERROR_TOKEN;
	}

	od_linear_alloc_t *_arena = yyextra->arena;
	char *s = od_linear_alloc_alloc(_arena, len + 1);
	if (s == NULL) {
		return ERROR_TOKEN;
	}
	memcpy(s, yytext + 1, len);
	s[len] = '\0';
	yylval->str = s;

	return IDENT;
}

{IDENT} {
	SET_YYLLOC();

	int tok = od_sql_minimal_keyword_lookup(yytext, (size_t)yyleng);
	if (tok != 0) {
		return tok;
	}

	od_linear_alloc_t *_arena = yyextra->arena;
	char *s = od_linear_alloc_alloc(_arena, (size_t)yyleng + 1);
	if (s == NULL) {
		return ERROR_TOKEN;
	}

	for (int _i = 0; _i < yyleng; _i++) {
		s[_i] = (char)tolower(yytext[_i]);
	}

	s[yyleng] = '\0';
	yylval->str = s;

	return IDENT;
}

. {
	SET_YYLLOC();
	return ERROR_TOKEN;
}

%%

void *
od_sql_minimal_yyalloc(yy_size_t bytes, yyscan_t yyscanner)
{
	od_sql_minimal_parse_ctx_t *ctx = od_sql_minimal_yyget_extra(yyscanner);
	return od_linear_alloc_alloc(ctx->arena, (size_t)bytes);
}

void *
od_sql_minimal_yyrealloc(void *ptr, yy_size_t bytes, yyscan_t yyscanner)
{
	(void)ptr;
	(void)bytes;
	(void)yyscanner;
	abort();
}

void
od_sql_minimal_yyfree(void *ptr, yyscan_t yyscanner)
{
	/* arena-owned memory -- nothing to do */
	(void)ptr;
	(void)yyscanner;
}
