%start top

%define api.pure full
%define api.prefix {od_sql_minimal_yy}
%define parse.error verbose
%locations

%parse-param { yyscan_t scanner }
%parse-param { od_sql_minimal_parse_ctx_t *ctx }
%lex-param   { yyscan_t scanner }

%code top {
#undef  YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(cur, rhs, n) \
    ((cur) = (n) ? YYRHSLOC(rhs, 1) : YYRHSLOC(rhs, 0))
}

%code requires {
	#include <odyssey.h>
	#include <od_memory.h>
	#include <sql/minimal/ast.h>

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
	typedef void *yyscan_t;
#endif

	typedef struct od_sql_minimal_parse_ctx od_sql_minimal_parse_ctx_t;

#ifndef YYSTYPE
#define YYSTYPE OD_SQL_MINIMAL_YYSTYPE
#endif

/*
 * Override bison's default YYLTYPE (struct with first_line/column etc.)
 * with a simple int (byte offset).  This goes into parse.tab.h and is
 * visible to both bison (.tab.c) and flex (scan.c via scan.h).
 */
#undef  YYLTYPE
typedef int od_sql_minimal_yyltype_t;
#define YYLTYPE od_sql_minimal_yyltype_t
}

%code {
	/* Bison defines #define YYLTYPE OD_SQL_MINIMAL_YYLTYPE above this
	 * block.  Override it with our int type after the fact.
	 * Also disable YYLTYPE_IS_TRIVIAL so bison doesn't emit
	 * struct-style { 1,1,1,1 } initializers for an int. */
#undef  YYLTYPE
#undef  OD_SQL_MINIMAL_YYLTYPE_IS_TRIVIAL
#define OD_SQL_MINIMAL_YYLTYPE_IS_TRIVIAL 0
typedef int od_sql_minimal_yyltype_t;
#define YYLTYPE od_sql_minimal_yyltype_t

	#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

	#include <sql/minimal/ctx.h>
	#include <sql/minimal/scan.h>

	static void od_sql_minimal_yyerror(YYLTYPE *loc,
					   yyscan_t scanner,
					   od_sql_minimal_parse_ctx_t *ctx,
					   const char *msg)
	{
		(void)loc;
		(void)scanner;

		ctx->had_error = 1;
		if (ctx->error_cb) {
			ctx->error_cb(msg, ctx->error_cb_userdata);
		}
	}

	static char *arena_str(od_sql_minimal_parse_ctx_t *ctx, const char *src,
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

	static char *arena_strcat(od_sql_minimal_parse_ctx_t *ctx, const char *a,
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
	((od_sql_minimal_ ## type_name ## _stmt_t *) \
	 od_sql_minimal_node_alloc((ctx)->arena, enum_val, \
			   sizeof(od_sql_minimal_ ## type_name ## _stmt_t)))

}

%union {
	char *str;
	int64_t ival;
	od_sql_minimal_node_t *node;
	od_sql_minimal_discard_target_t dtarget;
}

%token ERROR_TOKEN

%token <ival> INTEGER
%token <str>  SCONST
%token <str>  IDENT

/* session / admin */
%token KW_SET
%token KW_SHOW
%token KW_LOCAL
%token KW_SESSION
%token KW_TO
%token KW_DEFAULT
%token KW_ALL
%token KW_TRUE
%token KW_FALSE
%token KW_ON
%token KW_OFF
%token KW_BEGIN

%token KW_KILL_CLIENT
%token KW_RELOAD
%token KW_PAUSE
%token KW_RESUME
%token KW_DROP

/* transaction control (only BEGIN) */
%token KW_WORK
%token KW_TRANSACTION

/* prepared statements */
%token KW_DEALLOCATE
%token KW_PREPARE

/* discard */
%token KW_DISCARD
%token KW_TEMP
%token KW_TEMPORARY
%token KW_PLANS
%token KW_SEQUENCES
%token KW_UNLISTEN

%type <node> stmt
%type <node> set_stmt
%type <node> show_stmt
%type <node> kill_client_stmt
%type <node> reload_stmt
%type <node> pause_stmt
%type <node> resume_stmt
%type <node> drop_stmt
%type <node> begin_stmt
%type <node> deallocate_stmt
%type <node> discard_stmt
%type <node> unlisten_stmt
%type <str>  var_name
%type <str>  var_value
%type <str>  col_id
%type <dtarget> discard_target

%destructor { /* arena-allocated, no-op */ } <str>
%destructor { od_sql_minimal_node_free($$); } <node>

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
	  set_stmt
	| show_stmt
	| kill_client_stmt
	| reload_stmt
	| pause_stmt
	| resume_stmt
	| drop_stmt
	| begin_stmt
	| deallocate_stmt
	| discard_stmt
	| unlisten_stmt
	;

/*
 * SET [SESSION|LOCAL] name { TO | = } { value | DEFAULT }
 */
set_stmt:
	  KW_SET opt_set_scope var_name KW_TO var_value
		{
			od_sql_minimal_set_stmt_t *n =
				ALLOC_NODE(ctx, set, OD_SQL_MINIMAL_NODE_TYPE_SET_STMT);
			if (n == NULL) YYABORT;
			n->key   = $3; $3 = NULL;
			n->value = $5; $5 = NULL;
			$$ = (od_sql_minimal_node_t *)n;
		}
	| KW_SET opt_set_scope var_name '=' var_value
		{
			od_sql_minimal_set_stmt_t *n =
				ALLOC_NODE(ctx, set, OD_SQL_MINIMAL_NODE_TYPE_SET_STMT);
			if (n == NULL) YYABORT;
			n->key   = $3; $3 = NULL;
			n->value = $5; $5 = NULL;
			$$ = (od_sql_minimal_node_t *)n;
		}
	| KW_SET opt_set_scope var_name KW_TO KW_DEFAULT
		{
			od_sql_minimal_set_stmt_t *n =
				ALLOC_NODE(ctx, set, OD_SQL_MINIMAL_NODE_TYPE_SET_STMT);
			if (n == NULL) YYABORT;
			n->key   = $3; $3 = NULL;
			n->value = NULL;
			$$ = (od_sql_minimal_node_t *)n;
		}
	| KW_SET opt_set_scope var_name '=' KW_DEFAULT
		{
			od_sql_minimal_set_stmt_t *n =
				ALLOC_NODE(ctx, set, OD_SQL_MINIMAL_NODE_TYPE_SET_STMT);
			if (n == NULL) YYABORT;
			n->key   = $3; $3 = NULL;
			n->value = NULL;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

opt_set_scope:
	  %empty
	| KW_SESSION
	| KW_LOCAL
	;

show_stmt:
	  KW_SHOW var_name
		{
			od_sql_minimal_show_stmt_t *n =
				ALLOC_NODE(ctx, show, OD_SQL_MINIMAL_NODE_TYPE_SHOW_STMT);
			if (n == NULL) YYABORT;
			n->name = $2; $2 = NULL;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

/*
 * BEGIN [WORK|TRANSACTION]
 */
begin_stmt:
	  KW_BEGIN opt_transaction
		{
			od_sql_minimal_begin_stmt_t *n =
				ALLOC_NODE(ctx, begin, OD_SQL_MINIMAL_NODE_TYPE_BEGIN_STMT);
			if (n == NULL) YYABORT;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

opt_transaction:
	  %empty
	| KW_WORK
	| KW_TRANSACTION
	;

/*
 * DEALLOCATE [PREPARE] { name | ALL }
 */
deallocate_stmt:
	  KW_DEALLOCATE IDENT
		{
			od_sql_minimal_deallocate_stmt_t *n = ALLOC_NODE(ctx, deallocate,
				OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT);
			if (n == NULL) YYABORT;
			n->name   = $2; $2 = NULL;
			n->is_all = 0;
			$$ = (od_sql_minimal_node_t *)n;
		}
	| KW_DEALLOCATE KW_PREPARE IDENT
		{
			od_sql_minimal_deallocate_stmt_t *n = ALLOC_NODE(ctx, deallocate,
				OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT);
			if (n == NULL) YYABORT;
			n->name   = $3; $3 = NULL;
			n->is_all = 0;
			$$ = (od_sql_minimal_node_t *)n;
		}
	| KW_DEALLOCATE KW_ALL
		{
			od_sql_minimal_deallocate_stmt_t *n = ALLOC_NODE(ctx, deallocate,
				OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT);
			if (n == NULL) YYABORT;
			n->name   = NULL;
			n->is_all = 1;
			$$ = (od_sql_minimal_node_t *)n;
		}
	| KW_DEALLOCATE KW_PREPARE KW_ALL
		{
			od_sql_minimal_deallocate_stmt_t *n = ALLOC_NODE(ctx, deallocate,
				OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT);
			if (n == NULL) YYABORT;
			n->name   = NULL;
			n->is_all = 1;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

/*
 * DISCARD { ALL | TEMP | TEMPORARY | PLANS | SEQUENCES }
 */
discard_stmt:
	  KW_DISCARD discard_target
		{
			od_sql_minimal_discard_stmt_t *n = ALLOC_NODE(ctx, discard,
				OD_SQL_MINIMAL_NODE_TYPE_DISCARD_STMT);
			if (n == NULL) YYABORT;
			n->target = $2;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

discard_target:
	  KW_ALL         { $$ = OD_SQL_MINIMAL_DISCARD_ALL; }
	| KW_TEMP        { $$ = OD_SQL_MINIMAL_DISCARD_TEMP; }
	| KW_TEMPORARY   { $$ = OD_SQL_MINIMAL_DISCARD_TEMP; }
	| KW_PLANS       { $$ = OD_SQL_MINIMAL_DISCARD_PLANS; }
	| KW_SEQUENCES   { $$ = OD_SQL_MINIMAL_DISCARD_SEQUENCES; }
	;

/*
 * UNLISTEN { identifier | * }
 */
unlisten_stmt:
	  KW_UNLISTEN '*'
		{
			od_sql_minimal_unlisten_stmt_t *n = ALLOC_NODE(ctx, unlisten,
				OD_SQL_MINIMAL_NODE_TYPE_UNLISTEN_STMT);
			if (n == NULL) YYABORT;
			n->name   = NULL;
			n->is_all = 1;
			$$ = (od_sql_minimal_node_t *)n;
		}
	| KW_UNLISTEN col_id
		{
			od_sql_minimal_unlisten_stmt_t *n = ALLOC_NODE(ctx, unlisten,
				OD_SQL_MINIMAL_NODE_TYPE_UNLISTEN_STMT);
			if (n == NULL) YYABORT;
			n->name   = $2; $2 = NULL;
			n->is_all = 0;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

kill_client_stmt:
	  KW_KILL_CLIENT IDENT
		{
			if ($2 == NULL ||
			    strlen($2) != (size_t)(OD_ID_LEN + 1)) {
				od_sql_minimal_yyerror(&yylloc, scanner, ctx,
					       "invalid client id");
				YYABORT;
			}
			od_sql_minimal_kill_client_stmt_t *n = ALLOC_NODE(ctx,
				kill_client, OD_SQL_MINIMAL_NODE_TYPE_KILL_CLIENT_STMT);
			if (n == NULL) YYABORT;
			n->id = $2; $2 = NULL;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

reload_stmt:
	  KW_RELOAD
		{
			od_sql_minimal_reload_stmt_t *n = ALLOC_NODE(ctx, reload,
				OD_SQL_MINIMAL_NODE_TYPE_RELOAD_STMT);
			if (n == NULL) YYABORT;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

pause_stmt:
	  KW_PAUSE
		{
			od_sql_minimal_pause_stmt_t *n = ALLOC_NODE(ctx, pause,
				OD_SQL_MINIMAL_NODE_TYPE_PAUSE_STMT);
			if (n == NULL) YYABORT;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

resume_stmt:
	  KW_RESUME
		{
			od_sql_minimal_resume_stmt_t *n = ALLOC_NODE(ctx, resume,
				OD_SQL_MINIMAL_NODE_TYPE_RESUME_STMT);
			if (n == NULL) YYABORT;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

drop_stmt:
	  KW_DROP var_name
		{
			od_sql_minimal_drop_stmt_t *n = ALLOC_NODE(ctx, drop,
				OD_SQL_MINIMAL_NODE_TYPE_DROP_STMT);
			if (n == NULL) YYABORT;
			n->name = $2; $2 = NULL;
			$$ = (od_sql_minimal_node_t *)n;
		}
	;

var_name:
	  col_id                { $$ = $1; }
	| var_name '.' col_id
		{
			size_t la = strlen($1), lb = strlen($3);
			$$ = arena_strcat(ctx, $1, la, '.', $3, lb);
			if ($$ == NULL) {
				od_sql_minimal_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	;

var_value:
	  SCONST   { $$ = $1; $1 = NULL; }
	| col_id   { $$ = $1; }
	| KW_ON
		{
			$$ = arena_str(ctx, "on", 2);
			if ($$ == NULL) {
				od_sql_minimal_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	| KW_OFF
		{
			$$ = arena_str(ctx, "off", 3);
			if ($$ == NULL) {
				od_sql_minimal_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	| KW_TRUE
		{
			$$ = arena_str(ctx, "true", 4);
			if ($$ == NULL) {
				od_sql_minimal_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	| KW_FALSE
		{
			$$ = arena_str(ctx, "false", 5);
			if ($$ == NULL) {
				od_sql_minimal_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	| INTEGER
		{
			char tmp[24];
			int  n = snprintf(tmp, sizeof(tmp), "%" PRId64, $1);
			$$ = arena_str(ctx, tmp, (size_t)n);
			if ($$ == NULL) {
				od_sql_minimal_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	;

col_id:
	  IDENT
		{
			$$ = $1;
			$1 = NULL;
		}
	| KW_ALL
		{
			$$ = arena_str(ctx, "all", 3);
			if ($$ == NULL) {
				od_sql_minimal_yyerror(&yylloc, scanner, ctx,
					       "out of memory");
				YYABORT;
			}
		}
	;

%%
