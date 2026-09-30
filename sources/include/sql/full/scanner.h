#pragma once

#include <sql/full/keywords.h>

typedef union core_YYSTYPE
{
	int			ival;
	char	   *str;
	const char *keyword;
} core_YYSTYPE;

#ifndef YYLTYPE
#define YYLTYPE  int
#endif

typedef struct core_yy_extra_type
{
	char	   *scanbuf;
	Size		scanbuflen;

	const ScanKeywordList *keywordlist;
	const uint16 *keyword_tokens;

	int			backslash_quote;

	char	   *literalbuf;
	int			literallen;
	int			literalalloc;

	int			state_before_str_stop;
	int			xcdepth;
	char	   *dolqstart;
	YYLTYPE		save_yylloc;

	int32		utf16_first_part;

	bool		saw_non_ascii;
} core_yy_extra_type;

typedef void *core_yyscan_t;

typedef struct ScannerCallbackState
{
	core_yyscan_t yyscanner;
	int			location;
} ScannerCallbackState;

extern PGDLLIMPORT const uint16 ScanKeywordTokens[];

extern core_yyscan_t scanner_init(const char *str,
								  core_yy_extra_type *yyext,
								  const ScanKeywordList *keywordlist,
								  const uint16 *keyword_tokens);
extern void scanner_finish(core_yyscan_t yyscanner);
extern int	core_yylex(core_YYSTYPE *yylval_param, YYLTYPE *yylloc_param,
					   core_yyscan_t yyscanner);
extern int	scanner_errposition(int location, core_yyscan_t yyscanner);
extern void setup_scanner_errposition_callback(ScannerCallbackState *scbstate,
											   core_yyscan_t yyscanner,
											   int location);
extern void cancel_scanner_errposition_callback(ScannerCallbackState *scbstate);
pg_noreturn extern void scanner_yyerror(const char *message, core_yyscan_t yyscanner);

/* flex prefix is od_sql_full_yy, so the generated lexer function is od_sql_full_yylex */
extern int od_sql_full_yylex(core_YYSTYPE *yylval_param, YYLTYPE *yylloc_param,
							core_yyscan_t yyscanner);
#define core_yylex od_sql_full_yylex
