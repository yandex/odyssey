%option reentrant
%option bison-bridge
%option bison-locations
%option noyywrap
%option nodefault
%option nounput
%option noinput
%option extra-type="od_cfg_parse_ctx_t *"

%top{
/* flex-generated code triggers sign-compare warnings — suppress for this TU */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <odyssey.h>
#include <math.h>
#include <duration.h>
#include <cfg/parse.tab.h>
#include <cfg/ctx.h>
#include <cfg/keywords.h>
}

%{
static char *
od_cfg_lex_strdup(od_cfg_parse_ctx_t *ctx, const char *s, size_t len)
{
	(void)ctx;

	char *copy = od_malloc(len + 1);
	if (copy == NULL) {
		return NULL;
	}

	memcpy(copy, s, len);
	copy[len] = 0;

	return copy;
}

static char *
od_cfg_unescape_string(od_cfg_parse_ctx_t *ctx,
					   const char *s,
					   size_t len,
					   od_cfg_location_t loc)
{
	char *out = od_malloc(len + 1);
	if (out == NULL) {
		od_cfg_diag_error(ctx->diags, loc, "out of memory");
		return NULL;
	}

	size_t j = 0;
	for (size_t i = 0; i < len; i++) {
		if (s[i] == '\\' && i + 1 < len && s[i + 1] == '"') {
			out[j++] = '"';
			i++;
			continue;
		}
		out[j++] = s[i];
	}

	out[j] = 0;
	return out;
}

#undef YY_USER_ACTION
#define YY_USER_ACTION                                                \
	do {                                                              \
		yylloc->filename = yyextra->filename;                         \
		yylloc->first_line = yyextra->lexer_line;                     \
		yylloc->first_column = yyextra->lexer_column;                 \
		yylloc->first_offset = yyextra->lexer_offset;                 \
																	  \
		for (size_t i = 0; i < (size_t)yyleng; i++) {                 \
			if (yytext[i] == '\n') {                                  \
				yyextra->lexer_line++;                                \
				yyextra->lexer_column = 1;                            \
			} else {                                                  \
				yyextra->lexer_column++;                              \
			}                                                         \
			yyextra->lexer_offset++;                                  \
		}                                                             \
																	  \
		yylloc->last_line = yyextra->lexer_line;                      \
		yylloc->last_column = yyextra->lexer_column;                  \
		yylloc->last_offset = yyextra->lexer_offset;                  \
	} while (0);

%}

DIGIT       [0-9]
HEXDIGIT    [0-9a-fA-F]
IDENT_START [A-Za-z_]
IDENT_CONT  [A-Za-z0-9_]
IDENT       {IDENT_START}{IDENT_CONT}*
/*
 * Ported from PostgreSQL guc-file.l: INTEGER carries optional trailing
 * unit letters ("500ms", "1h", "0x10kB").
 */
NUMBER_SIGNED    [-+]?({DIGIT}+|0[xX]{HEXDIGIT}+)
SIZE_SUFFIX  [kK][bB]?|[mM][bB]?|[gG][bB]?|[bB]
/*
 * A PostgreSQL GUC-style time value: a number followed by at least one
 * unit letter ("500ms", "1h", "0x10s", "30min").  Unit validation and
 * the actual conversion happen in the token action (duration.c, ported
 * from PostgreSQL guc.c parse_int/convert_to_base_unit).
 *
 * Note: this rule is placed after the SIZE_NUMBER rule, so a size value
 * like "512m"/"512mb" still lexes as a size (the tie is won by rule
 * order), while unambiguous durations ("1min", "30s", "1h") win by
 * longest match.
 */
DUR_INTEGER  {NUMBER_SIGNED}[a-zA-Z]+
REAL         [-+]?({DIGIT}+[.]{DIGIT}*|[.]{DIGIT}+)([Ee][-+]?{DIGIT}+)?

%%

[ \t\r\n]+      ;

"#".*           ;

"{"             return '{';
"}"             return '}';

{NUMBER_SIGNED}{SIZE_SUFFIX} {
					errno = 0;
					char *suffix = NULL;
					/* decimal or 0x-hexadecimal; no octal */
					int base = 10;
					{
						const char *d = yytext;
						if (*d == '+' || *d == '-') {
							d++;
						}
						if (d[0] == '0' &&
						    (d[1] == 'x' ||
						     d[1] == 'X')) {
							base = 16;
						}
					}
					int64_t v = strtoll(yytext, &suffix, base);
					if (errno == ERANGE) {
						od_cfg_diag_error(yyextra->diags, *yylloc,
										  "number is out of range");
						return ERROR_TOKEN;
					}

					int64_t mult = 1;
					switch (*suffix) {
					case 'k': case 'K': mult = 1024LL; break;
					case 'm': case 'M': mult = 1024LL * 1024; break;
					case 'g': case 'G': mult = 1024LL * 1024 * 1024; break;
					case 'b': case 'B': mult = 1; break;
					default: break;
					}

					yylval->num = v * mult;
					return SIZE_NUMBER;
				}

{DUR_INTEGER}   {
					int64_t ms = 0;
					const char *hint = NULL;

					if (!od_duration_parse_ms(yytext, &ms,
								  &hint)) {
						if (hint != NULL) {
							od_cfg_diag_error_with_hint(
								yyextra->diags,
								*yylloc, hint,
								"invalid duration value '%s'",
								yytext);
						} else {
							od_cfg_diag_error(
								yyextra->diags,
								*yylloc,
								"invalid duration value '%s'",
								yytext);
						}
						return ERROR_TOKEN;
					}

					yylval->num = ms;
					return DURATION;
				}

{REAL}          {
					errno = 0;
					double v = strtod(yytext, NULL);
					if (errno == ERANGE || isnan(v) ||
					    isinf(v)) {
						od_cfg_diag_error(
							yyextra->diags,
							*yylloc,
							"number is out of range");
						return ERROR_TOKEN;
					}

					yylval->real = v;
					return REAL;
				}

{NUMBER_SIGNED}  {
					errno = 0;
					/* decimal or 0x-hexadecimal; no octal */
					int base = 10;
					{
						const char *d = yytext;
						if (*d == '+' || *d == '-') {
							d++;
						}
						if (d[0] == '0' &&
						    (d[1] == 'x' ||
						     d[1] == 'X')) {
							base = 16;
						}
					}
					int64_t v = strtoll(yytext, NULL, base);
					if (errno == ERANGE) {
						od_cfg_diag_error(yyextra->diags, *yylloc,
								  "number is out of range");
						return ERROR_TOKEN;
					}

					yylval->num = v;
					return NUMBER;
				}


\"([^\"]|\\\")*\" {
					assert(yyleng >= 2);
					size_t len = (size_t)yyleng - 2;

					char *copy = od_cfg_unescape_string(yyextra,
														 yytext + 1,
														 len,
														 *yylloc);
					 if (copy == NULL) {
						return ERROR_TOKEN;
					 }

					yylval->str = copy;
					return STRING;
				}

{IDENT}         {
					int tok = od_cfg_keyword_lookup(yytext, (size_t)yyleng);
					if (tok != 0) {
						return tok;
					}

					char *copy = od_cfg_lex_strdup(yyextra, yytext,
												   (size_t)yyleng);
					if (copy == NULL) {
						od_cfg_diag_error(yyextra->diags, *yylloc,
										  "out of memory");
						return ERROR_TOKEN;
					}

					od_free(yyextra->last_unknown_ident);
					yyextra->last_unknown_ident =
						od_cfg_lex_strdup(yyextra, yytext,
								  (size_t)yyleng);
					if (yyextra->last_unknown_ident == NULL) {
						od_cfg_diag_error(yyextra->diags, *yylloc,
								  "out of memory");
						od_free(copy);
						return ERROR_TOKEN;
					}

					yylval->str = copy;
					return IDENT;
				}

.               {
					od_cfg_diag_error(yyextra->diags, *yylloc,
									  "unexpected character '%c'", yytext[0]);
					return ERROR_TOKEN;
				}

%%
