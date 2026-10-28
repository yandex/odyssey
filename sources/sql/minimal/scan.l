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
%x TAIL

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
/*
 * scanner memory comes from the arena, which is wiped between parses by
 * od_linear_alloc_reset, so yyalloc does not zero:
 *  - yyguts_t and the buffer stack are explicitly memset() by flex itself
 *  - the yy_scan_bytes copy buffer is fully overwritten with the input
 *  - yy_scan_buffer assigns every yy_buffer_state field the scanner reads
 *    (yy_bs_lineno/yy_bs_column are only used by yylineno/yycolumn, which
 *    this scanner does not use)
 */
#define YY_FATAL_ERROR(msg) do { \
	od_sql_minimal_parse_ctx_t *_ctx = od_sql_minimal_yyget_extra(yyscanner); \
	if (_ctx && _ctx->fatal_set) { \
		_ctx->had_error = 1; \
		if (_ctx->error_cb) { \
			_ctx->error_cb(msg, _ctx->error_cb_userdata); \
		} \
		mm_longjmp(_ctx->fatal_jmp, 1); \
	} \
	fprintf(stderr, "%s\n", msg); \
	od_assert(0); \
} while (0)
}

%{

#define SET_YYLLOC() do { \
    yylloc->start = (int)(yytext - yyextra->scanbuf); \
    yylloc->end = yylloc->start + (int)yyleng; \
} while (0)

static void tail_touch(od_sql_minimal_parse_ctx_t *ctx, int start, int end,
                      int has_token)
{
    if (ctx->tail_location.start < 0) ctx->tail_location.start = start;
    ctx->tail_location.end = end;
    ctx->tail_has_token |= has_token;
}

#define TAIL_TOUCH(token) \
    tail_touch(yyextra, yylloc->start, yylloc->end, token)

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
<TAIL>[ \t\r\n\f]+ { SET_YYLLOC(); }
<TAIL>"--"[^\n]* {
    SET_YYLLOC();
    TAIL_TOUCH(0);
}
"/*"([^*]|\*+[^*/])*\*+"/" { SET_YYLLOC(); /* skip block comment */ }

 /*
  * unterminated block comment. Do not try to consume it with input(): on
  * EOF flex falls into yyrestart(yyin) with yyin == NULL, which re-enables
  * buffer filling, and the next scan step crashes in fread(). Report an
  * error instead, same as postgres does
  */
"/*"         { SET_YYLLOC(); return ERROR_TOKEN; }

<TAIL>"/*" {
    SET_YYLLOC();
    int depth = 1, previous = 0, c;
    while (depth && (c = input(yyscanner)) != 0) {
        yylloc->end++;
        if (previous == '/' && c == '*') { depth++; previous = 0; }
        else if (previous == '*' && c == '/') { depth--; previous = 0; }
        else previous = c;
    }
    if (depth) return ERROR_TOKEN;
    TAIL_TOUCH(0);
}

<TAIL>[eE]\'([^\'\\]|\\.|\'\')*\' {
    SET_YYLLOC(); TAIL_TOUCH(1);
}
<TAIL>\'([^\']|\'\')*\' {
    SET_YYLLOC(); TAIL_TOUCH(1);
}
<TAIL>\"([^\"]|\"\")*\" {
    SET_YYLLOC(); TAIL_TOUCH(1);
}
<TAIL>\$({IDENT_START}[A-Za-z0-9_\200-\377]*)?\$ {
    SET_YYLLOC();
    size_t length = (size_t)yyleng;
    char *delimiter = od_linear_alloc_alloc(yyextra->arena, length, 0);
    if (delimiter == NULL) return ERROR_TOKEN;
    memcpy(delimiter, yytext, length);
    size_t matched = 0;
    int c;
    while ((c = input(yyscanner)) != 0) {
        yylloc->end++;
        if ((unsigned char)c == (unsigned char)delimiter[matched]) matched++;
        else matched = c == '$' ? 1 : 0;
        if (matched == length) break;
    }
    if (matched != length) return ERROR_TOKEN;
    TAIL_TOUCH(1);
}
<TAIL>"(" {
    SET_YYLLOC(); yyextra->tail_depth++; TAIL_TOUCH(1);
}
<TAIL>")" {
    SET_YYLLOC();
    if (yyextra->tail_depth == 0) {
        yyless(0); BEGIN(INITIAL);
        if (yyextra->tail_has_token) {
            *yylloc = yyextra->tail_location;
            return RAW_SQL;
        }
    } else {
        yyextra->tail_depth--; TAIL_TOUCH(1);
    }
}
<TAIL>";" {
    SET_YYLLOC();
    if (yyextra->tail_depth == 0) {
        yyless(0); BEGIN(INITIAL);
        if (yyextra->tail_has_token) {
            *yylloc = yyextra->tail_location;
            return RAW_SQL;
        }
    } else TAIL_TOUCH(1);
}
<TAIL><<EOF>> {
    BEGIN(INITIAL);
    if (yyextra->tail_has_token) {
        *yylloc = yyextra->tail_location;
        return RAW_SQL;
    }
    return 0;
}
<TAIL>{IDENT_START}[A-Za-z0-9_$\200-\377]* {
    SET_YYLLOC(); TAIL_TOUCH(1);
}
<TAIL>[^ \t\r\n\f'"();$/A-Za-z0-9_\200-\377-]+ { SET_YYLLOC(); TAIL_TOUCH(1); }
<TAIL>. { SET_YYLLOC(); TAIL_TOUCH(1); }

";"          { SET_YYLLOC(); return ';'; }
"="          { SET_YYLLOC(); return '='; }
","          { SET_YYLLOC(); return ','; }
"("          { SET_YYLLOC(); return '('; }
")"          { SET_YYLLOC(); return ')'; }
"."          { SET_YYLLOC(); return '.'; }
"*"          { SET_YYLLOC(); return '*'; }

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
	char *s = od_linear_alloc_alloc(_arena, len + 1, 0);
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
	char *s = od_linear_alloc_alloc(_arena, len + 1, 0);
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
	od_linear_alloc_t *_arena = yyextra->arena;
	char *s = od_linear_alloc_alloc(_arena, (size_t)yyleng + 1, 0);
	if (s == NULL) {
		return ERROR_TOKEN;
	}

	/* IDENT is pure ASCII, no need for the locale-aware tolower() */
	for (int _i = 0; _i < yyleng; _i++) {
		char _c = yytext[_i];
		if (_c >= 'A' && _c <= 'Z') {
			_c += 'a' - 'A';
		}
		s[_i] = _c;
	}

	s[yyleng] = '\0';
	yylval->str = s;

	return tok != 0 ? tok : IDENT;
}

. {
	SET_YYLLOC();
	return ERROR_TOKEN;
}

%%

void od_sql_minimal_scan_tail(yyscan_t yyscanner)
{
    struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
    yyextra->tail_location = (od_sql_location_t){ -1, -1 };
    yyextra->tail_has_token = 0;
    yyextra->tail_depth = 0;
    BEGIN(TAIL);
}

void *
od_sql_minimal_yyalloc(yy_size_t bytes, yyscan_t yyscanner)
{
	od_sql_minimal_parse_ctx_t *ctx = od_sql_minimal_yyget_extra(yyscanner);
	return od_linear_alloc_alloc(ctx->arena, (size_t)bytes, 0);
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
