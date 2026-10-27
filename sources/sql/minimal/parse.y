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
    do { \
        (cur).start = (n) ? YYRHSLOC(rhs, 1).start : YYRHSLOC(rhs, 0).end; \
        (cur).end = YYRHSLOC(rhs, n).end; \
    } while (0)
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
 * with a pair of byte offsets.  This goes into parse.tab.h and is
 * visible to both bison (.tab.c) and flex (scan.c via scan.h).
 */
#undef  YYLTYPE
typedef od_sql_location_t od_sql_minimal_yyltype_t;
#define YYLTYPE od_sql_minimal_yyltype_t
}

%code {
	/* Bison defines #define YYLTYPE OD_SQL_MINIMAL_YYLTYPE above this
	 * block.  Override it with our byte range type after the fact.
	 * Also disable YYLTYPE_IS_TRIVIAL so bison doesn't emit
	 * line/column initializers for byte ranges. */
#undef  YYLTYPE
#undef  OD_SQL_MINIMAL_YYLTYPE_IS_TRIVIAL
#define OD_SQL_MINIMAL_YYLTYPE_IS_TRIVIAL 0
typedef od_sql_location_t od_sql_minimal_yyltype_t;
#define YYLTYPE od_sql_minimal_yyltype_t

	#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

	#include <sql/minimal/ctx.h>
	#include <sql/minimal/scan.h>
	void od_sql_minimal_scan_tail(yyscan_t scanner);

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
		char *s = od_linear_alloc_alloc(ctx->arena, len + 1, 0);
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
		char *s = od_linear_alloc_alloc(ctx->arena, la + 1 + lb + 1, 0);
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
	od_sql_location_t location;
	od_sql_minimal_node_t *node;
	od_sql_minimal_discard_target_t dtarget;
}

%token ERROR_TOKEN

%token <ival> INTEGER
%token <str>  SCONST
%token <str>  IDENT

/* session / admin */
%token <str> KW_SET
%token <str> KW_SHOW
%token <str> KW_LOCAL
%token <str> KW_SESSION
%token <str> KW_TO
%token <str> KW_DEFAULT
%token <str> KW_ALL
%token <str> KW_TRUE
%token <str> KW_FALSE
%token <str> KW_ON
%token <str> KW_OFF
%token <str> KW_BEGIN

/* transaction control (only BEGIN) */
%token <str> KW_WORK
%token <str> KW_TRANSACTION

/* prepared statements */
%token <str> KW_DEALLOCATE
%token <str> KW_PREPARE
%token <str> KW_EXECUTE
%token <str> KW_AS
%token RAW_SQL

/* discard */
%token <str> KW_DISCARD
%token <str> KW_TEMP
%token <str> KW_TEMPORARY
%token <str> KW_PLANS
%token <str> KW_SEQUENCES
%token <str> KW_UNLISTEN

%type <node> stmt
%type <node> set_stmt
%type <node> show_stmt
%type <node> begin_stmt
%type <node> deallocate_stmt
%type <node> discard_stmt
%type <node> unlisten_stmt
%type <node> prepare_stmt execute_stmt
%type <str>  var_name
%type <str>  var_value
%type <str>  col_id name unreserved_keyword
%type <location> prep_type_clause execute_param_clause
%type <dtarget> discard_target

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
	| begin_stmt
	| deallocate_stmt
	| discard_stmt
	| unlisten_stmt
	| prepare_stmt
	| execute_stmt
	;

/* Follow PostgreSQL's statement forms; retain ranges for opaque SQL tails. */
name:
      IDENT
    | unreserved_keyword
    ;

unreserved_keyword:
      KW_BEGIN
    | KW_DEALLOCATE
    | KW_DISCARD
    | KW_EXECUTE
    | KW_LOCAL
    | KW_OFF
    | KW_PLANS
    | KW_PREPARE
    | KW_SEQUENCES
    | KW_SESSION
    | KW_SET
    | KW_SHOW
    | KW_TEMP
    | KW_TEMPORARY
    | KW_TRANSACTION
    | KW_UNLISTEN
    | KW_WORK
    ;

prepare_stmt:
      KW_PREPARE name prep_type_clause KW_AS
        { od_sql_minimal_scan_tail(scanner); }
      RAW_SQL
        {
            od_sql_minimal_prepare_stmt_t *n = ALLOC_NODE(ctx, prepare,
                OD_SQL_MINIMAL_NODE_TYPE_PREPARE_STMT);
            if (n == NULL) YYABORT;
            n->name = $2;
            n->name_location = @2;
            n->types_location = $3;
            n->query_location = @6;
            $$ = (od_sql_minimal_node_t *)n;
        }
    ;

prep_type_clause:
      '(' { od_sql_minimal_scan_tail(scanner); } RAW_SQL ')' { $$ = @3; }
    | %empty { $$ = (od_sql_location_t){ -1, -1 }; }
    ;

execute_stmt:
      KW_EXECUTE name execute_param_clause
        {
            od_sql_minimal_execute_stmt_t *n = ALLOC_NODE(ctx, execute,
                OD_SQL_MINIMAL_NODE_TYPE_EXECUTE_STMT);
            if (n == NULL) YYABORT;
            n->name = $2;
            n->name_location = @2;
            n->args_location = $3;
            $$ = (od_sql_minimal_node_t *)n;
        }
    ;

execute_param_clause:
      '(' { od_sql_minimal_scan_tail(scanner); } RAW_SQL ')' { $$ = @$; }
    | %empty { $$ = (od_sql_location_t){ -1, -1 }; }
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
	  KW_DEALLOCATE name
		{
			od_sql_minimal_deallocate_stmt_t *n = ALLOC_NODE(ctx, deallocate,
				OD_SQL_MINIMAL_NODE_TYPE_DEALLOCATE_STMT);
			if (n == NULL) YYABORT;
			n->name   = $2; $2 = NULL;
			n->is_all = 0;
			$$ = (od_sql_minimal_node_t *)n;
		}
	| KW_DEALLOCATE KW_PREPARE name
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
