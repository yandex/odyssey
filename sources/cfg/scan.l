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
		if (s[i] != '\\') {
			out[j++] = s[i];
			continue;
		}

		if (i + 1 >= len) {
			od_cfg_diag_error(ctx->diags, loc, "unterminated escape sequence");
			od_free(out);
			return NULL;
		}

		char c = s[++i];
		switch (c) {
		case '\\': out[j++] = '\\'; break;
		case '"':  out[j++] = '"';  break;
		case 'n':  out[j++] = '\n'; break;
		case 'r':  out[j++] = '\r'; break;
		case 't':  out[j++] = '\t'; break;
		default:
			od_cfg_diag_error(ctx->diags, loc,
							  "unknown escape sequence '\\%c'", c);
			od_free(out);
			return NULL;
		}
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
IDENT_START [A-Za-z_]
IDENT_CONT  [A-Za-z0-9_]
IDENT       {IDENT_START}{IDENT_CONT}*
NUMBER_SIGNED    -?{DIGIT}+
SIZE_SUFFIX  [kK][bB]?|[mM][bB]?|[gG][bB]?|[bB]

%%

[ \t\r\n]+      ;

"#".*           ;

"{"             return '{';
"}"             return '}';

{NUMBER_SIGNED}{SIZE_SUFFIX} {
					errno = 0;
					char *suffix = NULL;
					int64_t v = strtoll(yytext, &suffix, 10);
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

{NUMBER_SIGNED}  {
					errno = 0;
					int64_t v = strtoll(yytext, NULL, 10);
					if (errno == ERANGE) {
						od_cfg_diag_error(yyextra->diags, *yylloc,
										  "number is out of range");
						return ERROR_TOKEN;
					}

					yylval->num = v;
					return NUMBER;
				}


\"([^\\\"]|\\.)*\" {
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
					if (tok != 0)
						return tok;

					char *copy = od_cfg_lex_strdup(yyextra, yytext,
												   (size_t)yyleng);
					if (copy == NULL) {
						od_cfg_diag_error(yyextra->diags, *yylloc,
										  "out of memory");
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
