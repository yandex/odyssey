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
<TAIL>(.|\n) {
    /*
     * The tail between "EXECUTE x(" / "PREPARE x(..) AS" and the
     * terminator is opaque: no tokens are produced from it, only one
     * RAW_SQL span. Scan it with a raw byte loop directly over the
     * scan buffer, staying aware of comments, quoting and nested
     * parentheses, and stop at the first statement/argument
     * terminator. Keeping the tail in a single flex rule keeps the
     * -CF scanner tables (and thus the fast path of every other
     * query) small.
     */
    const char *b = yyextra->scanbuf;
    size_t n = yyextra->scanbuflen;
    size_t first = (size_t)(yytext - b);
    size_t i = first;
    int span_start = -1, span_end = -1;
    int has_token = 0;
    int paren_depth = 0;
    size_t term = n;

#define TAIL_SPAN_TOUCH(s, e, tok) do { \
    if (span_start < 0) span_start = (int)(s); \
    span_end = (int)(e); \
    has_token |= (tok); \
} while (0)

    while (i < n) {
        size_t elem = i;
        char c = b[i];
        switch (c) {
        case ' ': case '\t': case '\r': case '\n': case '\f':
            i++;
            continue;
        case '-':
            if (i + 1 < n && b[i + 1] == '-') {
                /* line comment */
                i += 2;
                while (i < n && b[i] != '\n') i++;
                TAIL_SPAN_TOUCH(elem, i, 0);
                continue;
            }
            i++;
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        case '/':
            if (i + 1 < n && b[i + 1] == '*') {
                /* nested block comment */
                i += 2;
                int prev = 0, depth = 1;
                while (depth > 0 && i < n) {
                    char cc = b[i++];
                    if (prev == '/' && cc == '*') { depth++; prev = 0; }
                    else if (prev == '*' && cc == '/') { depth--; prev = 0; }
                    else prev = cc;
                }
                if (depth > 0) return ERROR_TOKEN;
                TAIL_SPAN_TOUCH(elem, i, 0);
                continue;
            }
            i++;
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        case '\'':
        case '"': {
            char q = c;
            i++;
            while (i < n) {
                if (b[i] == q) {
                    if (i + 1 < n && b[i + 1] == q) { i += 2; continue; }
                    i++;
                    goto quoted_closed;
                }
                i++;
            }
            /* unterminated quote: no special treatment from here on */
            i = elem + 1;
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        quoted_closed:
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        }
        case '$': {
            /* dollar quote: $tag$ ... $tag$ */
            size_t j = i + 1;
            if (j < n && b[j] != '$') {
                char cj = b[j];
                if (!((cj >= 'A' && cj <= 'Z') ||
                      (cj >= 'a' && cj <= 'z') ||
                      cj == '_' || (unsigned char)cj >= 0200)) {
                    /* not a dollar quote */
                    i++;
                    TAIL_SPAN_TOUCH(elem, i, 1);
                    continue;
                }
                j++;
                while (j < n) {
                    cj = b[j];
                    if (cj == '$') break;
                    if (!((cj >= '0' && cj <= '9') ||
                          (cj >= 'A' && cj <= 'Z') ||
                          (cj >= 'a' && cj <= 'z') ||
                          cj == '_' || (unsigned char)cj >= 0200))
                        break;
                    j++;
                }
            }
            if (j >= n || b[j] != '$') {
                /* no closing $: not a dollar quote */
                i++;
                TAIL_SPAN_TOUCH(elem, i, 1);
                continue;
            }
            size_t dlen = j - i + 1;
            size_t m = 0;
            size_t k = j + 1;
            while (k < n) {
                if ((unsigned char)b[k] == (unsigned char)b[i + m]) m++;
                else m = (b[k] == '$') ? 1 : 0;
                if (m == dlen) { k++; break; }
                k++;
            }
            if (m != dlen) return ERROR_TOKEN;
            i = k;
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        }
        case '(':
            paren_depth++;
            i++;
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        case ')':
            if (paren_depth == 0) { term = i; goto tail_done; }
            paren_depth--;
            i++;
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        case ';':
            if (paren_depth == 0) { term = i; goto tail_done; }
            i++;
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        default:
            i++;
            TAIL_SPAN_TOUCH(elem, i, 1);
            continue;
        }
    }
    term = n;

tail_done:
#undef TAIL_SPAN_TOUCH
    yyless((int)(term - first));
    BEGIN(INITIAL);
    if (has_token) {
        yylloc->start = span_start;
        yylloc->end = span_end;
        return RAW_SQL;
    }
    /*
     * Nothing but comments/whitespace in the tail: do not emit
     * RAW_SQL, rescan delivers the terminator to the grammar as
     * usual (which reports the syntax error).
     */
}


"/*"         { SET_YYLLOC();
               int depth = 1, previous = 0, c;
               while (depth && (c = input(yyscanner)) != 0) {
                   if (previous == '/' && c == '*') { depth++; previous = 0; }
                   else if (previous == '*' && c == '/') { depth--; previous = 0; }
                   else previous = c;
               }
               if (depth) return ERROR_TOKEN;
               /* skip block comment */
             }

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
