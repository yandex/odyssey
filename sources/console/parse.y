%start top

%define api.pure full
%define api.prefix {od_console_yy}
%define parse.error verbose
%locations

%parse-param { yyscan_t scanner }
%parse-param { od_console_parse_ctx_t *ctx }
%lex-param   { yyscan_t scanner }

%code top {
#undef  YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(cur, rhs, n) \
    ((cur) = (n) ? YYRHSLOC(rhs, 1) : YYRHSLOC(rhs, 0))
}

%code requires {
	#include <odyssey.h>
	#include <od_memory.h>
	#include <console/ast.h>

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
	typedef void *yyscan_t;
#endif

	typedef struct od_console_parse_ctx od_console_parse_ctx_t;

#ifndef YYSTYPE
#define YYSTYPE OD_CONSOLE_YYSTYPE
#endif

#undef  YYLTYPE
typedef int od_console_yyltype_t;
#define YYLTYPE od_console_yyltype_t
}

%code {
#undef  YYLTYPE
#undef  OD_CONSOLE_YYLTYPE_IS_TRIVIAL
#define OD_CONSOLE_YYLTYPE_IS_TRIVIAL 0
typedef int od_console_yyltype_t;
#define YYLTYPE od_console_yyltype_t

	#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

	#include <console/ctx.h>
	#include <console/scan.h>

	static void od_console_yyerror(YYLTYPE *loc, yyscan_t scanner,
				       od_console_parse_ctx_t *ctx,
				       const char *msg)
	{
		(void)loc;
		(void)scanner;

		ctx->had_error = 1;
		if (ctx->error_cb) {
			ctx->error_cb(msg, ctx->error_cb_userdata);
		}
	}

	static char *arena_str(od_console_parse_ctx_t *ctx, const char *src,
			       size_t len)
	{
		char *s = od_linear_alloc_alloc(ctx->arena, len + 1);
		if (s == NULL) {
			return NULL;
		}
		memcpy(s, src, len);
		s[len] = '\0';
		return s;
	}

	static char *arena_strcat(od_console_parse_ctx_t *ctx, const char *a,
				  size_t la, char sep, const char *b,
				  size_t lb)
	{
		char *s = od_linear_alloc_alloc(ctx->arena, la + 1 + lb + 1);
		if (s == NULL) {
			return NULL;
		}
		memcpy(s, a, la);
		s[la] = sep;
		memcpy(s + la + 1, b, lb);
		s[la + 1 + lb] = '\0';
		return s;
	}

#define ALLOC_NODE(ctx, type_name, enum_val) \
	((od_console_ ## type_name ## _stmt_t *) \
	 od_console_node_alloc((ctx)->arena, enum_val, \
			   sizeof(od_console_ ## type_name ## _stmt_t)))
}

%union {
	char *str;
	int64_t ival;
	od_console_node_t *node;
}

%token ERROR_TOKEN

%token <ival> INTEGER
%token <str>  SCONST
%token <str>  IDENT

/* top-level commands */
%token KW_SHOW
%token KW_KILL_CLIENT
%token KW_RELOAD
%token KW_PAUSE
%token KW_RESUME
%token KW_SET
%token KW_DROP
%token KW_GC

/* set helpers */
%token KW_TO
%token KW_DEFAULT

/* drop targets */
%token KW_SERVERS

%type <node> stmt
%type <node> show_stmt
%type <node> kill_client_stmt
%type <node> reload_stmt
%type <node> pause_stmt
%type <node> resume_stmt
%type <node> set_stmt
%type <node> drop_stmt
%type <node> gc_stmt
%type <str> set_key
%type <str> set_value
%type <str> col_id
%type <str> opt_show_arg

%destructor { /* arena-allocated, no-op */ } <str>
%destructor { od_console_node_free($$); } <node>

%%

top:
	  stmt opt_semicolon { ctx->result = $1; }
	| opt_semicolon      { ctx->result = NULL; }
	;

opt_semicolon:
	  %empty
	| ';'
	;

stmt:
	  show_stmt
	| kill_client_stmt
	| reload_stmt
	| pause_stmt
	| resume_stmt
	| set_stmt
	| drop_stmt
	| gc_stmt
	;

/*
 * SHOW <name> [<arg>]
 *
 * Currently the optional <arg> is used by SHOW CONFIG <field_name>
 * to filter the output to a single configuration parameter.
 */
show_stmt:
	  KW_SHOW col_id opt_show_arg
		{
			od_console_show_stmt_t *n =
				ALLOC_NODE(ctx, show, OD_CONSOLE_NODE_TYPE_SHOW_STMT);
			if (n == NULL) YYABORT;
			n->name = $2; $2 = NULL;
			n->arg  = $3; $3 = NULL;
			$$ = (od_console_node_t *)n;
		}
	;

opt_show_arg:
	  %empty    { $$ = NULL; }
	| col_id    { $$ = $1; $1 = NULL; }
	;

/*
 * KILL_CLIENT <id>
 */
kill_client_stmt:
	  KW_KILL_CLIENT IDENT
		{
			if ($2 == NULL ||
			    strlen($2) != (size_t)(OD_ID_LEN + 1)) {
				od_console_yyerror(&yylloc, scanner, ctx,
					       "invalid client id");
				YYABORT;
			}
			od_console_kill_client_stmt_t *n = ALLOC_NODE(ctx,
				kill_client, OD_CONSOLE_NODE_TYPE_KILL_CLIENT_STMT);
			if (n == NULL) YYABORT;
			n->id = $2; $2 = NULL;
			$$ = (od_console_node_t *)n;
		}
	;

/*
 * RELOAD
 */
reload_stmt:
	  KW_RELOAD
		{
			od_console_reload_stmt_t *n = ALLOC_NODE(ctx, reload,
				OD_CONSOLE_NODE_TYPE_RELOAD_STMT);
			if (n == NULL) YYABORT;
			$$ = (od_console_node_t *)n;
		}
	;

/*
 * PAUSE
 */
pause_stmt:
	  KW_PAUSE
		{
			od_console_pause_stmt_t *n = ALLOC_NODE(ctx, pause,
				OD_CONSOLE_NODE_TYPE_PAUSE_STMT);
			if (n == NULL) YYABORT;
			$$ = (od_console_node_t *)n;
		}
	;

/*
 * RESUME
 */
resume_stmt:
	  KW_RESUME
		{
			od_console_resume_stmt_t *n = ALLOC_NODE(ctx, resume,
				OD_CONSOLE_NODE_TYPE_RESUME_STMT);
			if (n == NULL) YYABORT;
			$$ = (od_console_node_t *)n;
		}
	;

/*
 * SET key [= | TO] [value | DEFAULT]
 *
 * The console SET command accepts key=value syntax. The value is
 * parsed but currently ignored by the console handler — it just
 * replies with success.
 */
set_stmt:
	  KW_SET set_key '=' set_value
		{
			od_console_set_stmt_t *n =
				ALLOC_NODE(ctx, set, OD_CONSOLE_NODE_TYPE_SET_STMT);
			if (n == NULL) YYABORT;
			n->key   = $2; $2 = NULL;
			n->value = $4; $4 = NULL;
			$$ = (od_console_node_t *)n;
		}
	| KW_SET set_key KW_TO set_value
		{
			od_console_set_stmt_t *n =
				ALLOC_NODE(ctx, set, OD_CONSOLE_NODE_TYPE_SET_STMT);
			if (n == NULL) YYABORT;
			n->key   = $2; $2 = NULL;
			n->value = $4; $4 = NULL;
			$$ = (od_console_node_t *)n;
		}
	| KW_SET set_key '=' KW_DEFAULT
		{
			od_console_set_stmt_t *n =
				ALLOC_NODE(ctx, set, OD_CONSOLE_NODE_TYPE_SET_STMT);
			if (n == NULL) YYABORT;
			n->key   = $2; $2 = NULL;
			n->value = NULL;
			$$ = (od_console_node_t *)n;
		}
	| KW_SET set_key KW_TO KW_DEFAULT
		{
			od_console_set_stmt_t *n =
				ALLOC_NODE(ctx, set, OD_CONSOLE_NODE_TYPE_SET_STMT);
			if (n == NULL) YYABORT;
			n->key   = $2; $2 = NULL;
			n->value = NULL;
			$$ = (od_console_node_t *)n;
		}
	;

set_key:
	  col_id                { $$ = $1; }
	| set_key '.' col_id
		{
			size_t la = strlen($1), lb = strlen($3);
			$$ = arena_strcat(ctx, $1, la, '.', $3, lb);
			if ($$ == NULL) {
				od_console_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	;

set_value:
	  SCONST   { $$ = $1; $1 = NULL; }
	| col_id   { $$ = $1; }
	| INTEGER
		{
			char tmp[24];
			int  n = snprintf(tmp, sizeof(tmp), "%" PRId64, $1);
			$$ = arena_str(ctx, tmp, (size_t)n);
			if ($$ == NULL) {
				od_console_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	;

/*
 * DROP SERVERS
 */
drop_stmt:
	  KW_DROP KW_SERVERS
		{
			od_console_drop_stmt_t *n =
				ALLOC_NODE(ctx, drop, OD_CONSOLE_NODE_TYPE_DROP_STMT);
			if (n == NULL) YYABORT;
			n->target = OD_CONSOLE_DROP_SERVERS;
			n->path   = NULL;
			$$ = (od_console_node_t *)n;
		}
	;

/*
 * GC
 */
gc_stmt:
	  KW_GC
		{
			od_console_gc_stmt_t *n = ALLOC_NODE(ctx, gc,
				OD_CONSOLE_NODE_TYPE_GC_STMT);
			if (n == NULL) YYABORT;
			$$ = (od_console_node_t *)n;
		}
	;

col_id:
	  IDENT
		{
			$$ = $1;
			$1 = NULL;
		}
	| KW_SERVERS
		{
			$$ = arena_str(ctx, "servers", 7);
			if ($$ == NULL) {
				od_console_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	;

%%
