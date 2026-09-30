#include <odyssey.h>
#include <tests/odyssey_test.h>
#include <pthread.h>
#include <string.h>

#include <sql/full/parser.h>
#include <sql/full/ast.h>
#include <sql/full/pg_constants.h>
#include <sql/full/gramparse.h>
#include <alloc/linear.h>

#define OD_TEST_FULL_PARSE_ARENA_SIZE (5 * 1024 * 1024)

static _Alignas(max_align_t) uint8_t s_arena_buf[OD_TEST_FULL_PARSE_ARENA_SIZE];
static od_linear_alloc_t s_arena;

static char s_err_buf[1024];

static void on_error(const char *msg, void *userdata)
{
	(void)userdata;
	strncpy(s_err_buf, msg, sizeof(s_err_buf) - 1);
	s_err_buf[sizeof(s_err_buf) - 1] = '\0';
}

static List *parse_ok(const char *input)
{
	s_err_buf[0] = '\0';
	List *result = od_sql_full_parse(&s_arena, input, strlen(input),
					 on_error, NULL);
	if (result == NULL || s_err_buf[0] != '\0') {
		fprintf(stderr,
			"parse_ok FAILED: input=[%s] err=[%s] result=%p\n",
			input, s_err_buf, (void *)result);
		abort();
	}
	return result;
}

static void parse_fail(const char *input)
{
	s_err_buf[0] = '\0';
	List *result = od_sql_full_parse(&s_arena, input, strlen(input),
					 on_error, NULL);
	if (result != NULL) {
		fprintf(stderr,
			"parse_fail: expected failure but got result for [%s]\n",
			input);
		abort();
	}
	test(result == NULL);
}

static void test_select_star(void)
{
	List *tree = parse_ok("SELECT * FROM t");
	test(tree != NULL);
	test(list_length(tree) == 1);
	RawStmt *rs = (RawStmt *)linitial(tree);
	test(rs != NULL);
	test(IsA(rs->stmt, SelectStmt));
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(s->targetList != NULL);
	test(s->fromClause != NULL);
}

static void test_select_columns(void)
{
	List *tree = parse_ok("SELECT a, b, c FROM t");
	test(tree != NULL);
	RawStmt *rs = (RawStmt *)linitial(tree);
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(list_length(s->targetList) == 3);
}

static void test_select_where(void)
{
	List *tree = parse_ok("SELECT * FROM t WHERE a = 1");
	test(tree != NULL);
	RawStmt *rs = (RawStmt *)linitial(tree);
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(s->whereClause != NULL);
	test(IsA(s->whereClause, A_Expr));
}

static void test_select_join(void)
{
	List *tree = parse_ok("SELECT * FROM a JOIN b ON a.x = b.y");
	test(tree != NULL);
	RawStmt *rs = (RawStmt *)linitial(tree);
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(s->fromClause != NULL);
	test(list_length(s->fromClause) == 1);
}

static void test_select_left_join(void)
{
	parse_ok("SELECT * FROM a LEFT JOIN b ON a.x = b.y");
}

static void test_select_subquery(void)
{
	parse_ok("SELECT * FROM (SELECT * FROM t) AS sub");
}

static void test_select_order_by(void)
{
	List *tree = parse_ok("SELECT * FROM t ORDER BY a DESC, b ASC");
	test(tree != NULL);
	RawStmt *rs = (RawStmt *)linitial(tree);
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(s->sortClause != NULL);
	test(list_length(s->sortClause) == 2);
}

static void test_select_limit_offset(void)
{
	List *tree = parse_ok("SELECT * FROM t LIMIT 10 OFFSET 5");
	test(tree != NULL);
	RawStmt *rs = (RawStmt *)linitial(tree);
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(s->limitCount != NULL);
	test(s->limitOffset != NULL);
}

static void test_select_group_having(void)
{
	List *tree = parse_ok(
		"SELECT a, count(*) FROM t GROUP BY a HAVING count(*) > 1");
	test(tree != NULL);
	RawStmt *rs = (RawStmt *)linitial(tree);
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(s->groupClause != NULL);
	test(s->havingClause != NULL);
}

static void test_select_union(void)
{
	List *tree = parse_ok("SELECT * FROM a UNION SELECT * FROM b");
	test(tree != NULL);
	RawStmt *rs = (RawStmt *)linitial(tree);
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(s->op != 0 || s->larg != NULL);
	test(s->larg != NULL);
	test(s->rarg != NULL);
}

static void test_select_distinct(void)
{
	List *tree = parse_ok("SELECT DISTINCT a, b FROM t");
	test(tree != NULL);
	RawStmt *rs = (RawStmt *)linitial(tree);
	SelectStmt *s = (SelectStmt *)rs->stmt;
	test(s->distinctClause != NULL);
}

static void test_select_constants(void)
{
	parse_ok("SELECT 1");
	parse_ok("SELECT 1, 2");
	parse_ok("SELECT 'hello'");
	parse_ok("SELECT true");
	parse_ok("SELECT 1, 2.5, 'hello', true, false, NULL");
}

static void test_select_qualified_name(void)
{
	parse_ok("SELECT * FROM schema.table");
	parse_ok("SELECT * FROM db.schema.table");
}

static void test_select_alias(void)
{
	parse_ok("SELECT a AS x FROM t AS alias_t");
	parse_ok("SELECT a x FROM t alias_t");
}

static void test_select_expr(void)
{
	parse_ok("SELECT a + b * c FROM t WHERE x = y AND z > 10 OR w IS NULL");
}

static void test_select_case_insensitive(void)
{
	parse_ok("select * from t");
	parse_ok("SeLeCt * FrOm t");
}

static void test_select_comments(void)
{
	parse_ok("SELECT * FROM t -- comment\n");
	parse_ok("SELECT /* block comment */ * FROM t");
}

static void test_select_string_escape(void)
{
	parse_ok("SELECT 'it''s a string' FROM t");
}

static void test_select_dollar_quote(void)
{
	parse_ok("SELECT $$dollar quoted$$ FROM t");
}

static void test_parse_error(void)
{
	parse_fail("SELECT FROM");
	parse_fail("SELECT * WHERE");
	parse_fail("SELECT * FROM t ORDER");
	parse_fail("GARBAGE SQL");
}

static void test_select_typecast(void)
{
	parse_ok("SELECT 1::integer FROM t");
	parse_ok("SELECT '2024-01-01'::date FROM t");
}

/*
 * Deep parse-tree assertions.  The helpers below assume a successful parse
 * of a single SELECT statement and verify the exact shape of the produced
 * AST: node tags, lists, names and constant values.
 */

static SelectStmt *parse_select_one(const char *input)
{
	List *tree = parse_ok(input);
	test(tree != NULL);
	test(list_length(tree) == 1);
	RawStmt *rs = linitial(tree);
	test(rs != NULL);
	test(IsA(rs, RawStmt));
	test(rs->stmt != NULL);
	test(IsA(rs->stmt, SelectStmt));
	return (SelectStmt *)rs->stmt;
}

static ColumnRef *expect_column_ref(Node *node, const char *field)
{
	test(node != NULL);
	test(IsA(node, ColumnRef));
	ColumnRef *cr = (ColumnRef *)node;
	test(cr->fields != NULL);
	test(list_length(cr->fields) == 1);
	test(strcmp(strVal(linitial(cr->fields)), field) == 0);
	return cr;
}

static void expect_int_const(Node *node, int ival)
{
	test(node != NULL);
	test(IsA(node, A_Const));
	A_Const *c = (A_Const *)node;
	test(c->val.ival.type == T_Integer);
	test(c->val.ival.ival == ival);
}

static void expect_string_const(Node *node, const char *sval)
{
	test(node != NULL);
	test(IsA(node, A_Const));
	A_Const *c = (A_Const *)node;
	test(c->val.sval.type == T_String);
	test(strcmp(c->val.sval.sval, sval) == 0);
}

static void expect_simple_a_expr(Node *node, const char *op)
{
	test(node != NULL);
	test(IsA(node, A_Expr));
	A_Expr *e = (A_Expr *)node;
	test(e->kind == AEXPR_OP);
	test(e->name != NULL);
	test(list_length(e->name) == 1);
	test(strcmp(strVal(linitial(e->name)), op) == 0);
}

static void test_tree_select_star(void)
{
	SelectStmt *s = parse_select_one("SELECT * FROM t");

	test(list_length(s->targetList) == 1);
	ResTarget *rt = linitial(s->targetList);
	test(IsA(rt, ResTarget));
	test(rt->name == NULL);
	test(rt->indirection == NULL);
	test(rt->val != NULL);
	test(IsA(rt->val, ColumnRef));
	ColumnRef *cr = (ColumnRef *)rt->val;
	test(list_length(cr->fields) == 1);
	test(IsA(linitial(cr->fields), A_Star));

	test(list_length(s->fromClause) == 1);
	RangeVar *rv = linitial(s->fromClause);
	test(IsA(rv, RangeVar));
	test(rv->catalogname == NULL);
	test(rv->schemaname == NULL);
	test(strcmp(rv->relname, "t") == 0);
	test(rv->alias == NULL);
}

static void test_tree_select_columns(void)
{
	SelectStmt *s = parse_select_one("SELECT a, b, c FROM t");

	test(list_length(s->targetList) == 3);
	expect_column_ref(((ResTarget *)linitial(s->targetList))->val, "a");
	expect_column_ref(((ResTarget *)lsecond(s->targetList))->val, "b");
	expect_column_ref(((ResTarget *)llast(s->targetList))->val, "c");
}

static void test_tree_select_where(void)
{
	SelectStmt *s = parse_select_one("SELECT * FROM t WHERE a = 1");

	expect_simple_a_expr(s->whereClause, "=");
	A_Expr *e = (A_Expr *)s->whereClause;
	expect_column_ref(e->lexpr, "a");
	expect_int_const(e->rexpr, 1);
}

static void test_tree_select_bool_and_null(void)
{
	SelectStmt *s = parse_select_one(
		"SELECT * FROM t WHERE x = y AND z > 10 OR w IS NULL");

	BoolExpr *or_expr = (BoolExpr *)s->whereClause;
	test(IsA(or_expr, BoolExpr));
	test(or_expr->boolop == OR_EXPR);
	test(or_expr->args != NULL);
	test(list_length(or_expr->args) == 2);

	BoolExpr *and_expr = linitial(or_expr->args);
	test(IsA(and_expr, BoolExpr));
	test(and_expr->boolop == AND_EXPR);
	test(list_length(and_expr->args) == 2);

	expect_simple_a_expr(linitial(and_expr->args), "=");
	expect_simple_a_expr(lsecond(and_expr->args), ">");

	NullTest *nt = lsecond(or_expr->args);
	test(IsA(nt, NullTest));
	test(nt->nulltesttype == IS_NULL);
	expect_column_ref((Node *)nt->arg, "w");
}

static void test_tree_select_constants(void)
{
	SelectStmt *s =
		parse_select_one("SELECT 1, 2.5, 'hello', true, false, NULL");

	test(list_length(s->targetList) == 6);

	A_Const *c = (A_Const *)((ResTarget *)linitial(s->targetList))->val;
	test(IsA(c, A_Const));
	test(c->val.ival.type == T_Integer);
	test(c->val.ival.ival == 1);

	c = (A_Const *)((ResTarget *)lsecond(s->targetList))->val;
	test(IsA(c, A_Const));
	test(c->val.fval.type == T_Float);
	test(strcmp(c->val.fval.fval, "2.5") == 0);

	c = (A_Const *)((ResTarget *)lthird(s->targetList))->val;
	test(IsA(c, A_Const));
	test(c->val.sval.type == T_String);
	test(strcmp(c->val.sval.sval, "hello") == 0);

	c = (A_Const *)((ResTarget *)lfourth(s->targetList))->val;
	test(IsA(c, A_Const));
	test(c->val.boolval.type == T_Boolean);
	test(c->val.boolval.boolval == true);

	c = (A_Const *)((ResTarget *)list_nth(s->targetList, 4))->val;
	test(IsA(c, A_Const));
	test(c->val.boolval.type == T_Boolean);
	test(c->val.boolval.boolval == false);

	c = (A_Const *)((ResTarget *)llast(s->targetList))->val;
	test(IsA(c, A_Const));
	test(c->isnull == true);
}

static void test_tree_select_func_call(void)
{
	SelectStmt *s = parse_select_one("SELECT count(*) FROM t");

	ResTarget *rt = linitial(s->targetList);
	test(rt->val != NULL);
	test(IsA(rt->val, FuncCall));
	FuncCall *fc = (FuncCall *)rt->val;
	test(list_length(fc->funcname) == 1);
	test(strcmp(strVal(linitial(fc->funcname)), "count") == 0);
	test(fc->agg_star == true);
	test(fc->agg_distinct == false);
	test(fc->args == NIL);
	test(fc->over == NULL);

	s = parse_select_one("SELECT count(DISTINCT a) FROM t");
	rt = linitial(s->targetList);
	test(IsA(rt->val, FuncCall));
	fc = (FuncCall *)rt->val;
	test(strcmp(strVal(linitial(fc->funcname)), "count") == 0);
	test(fc->agg_star == false);
	test(fc->agg_distinct == true);
	test(list_length(fc->args) == 1);
	expect_column_ref(linitial(fc->args), "a");
}

static void test_tree_select_having(void)
{
	SelectStmt *s = parse_select_one(
		"SELECT a, count(*) FROM t GROUP BY a HAVING count(*) > 1");

	test(list_length(s->groupClause) == 1);
	expect_column_ref(linitial(s->groupClause), "a");

	expect_simple_a_expr(s->havingClause, ">");
	A_Expr *e = (A_Expr *)s->havingClause;
	test(IsA(e->lexpr, FuncCall));
	FuncCall *fc = (FuncCall *)e->lexpr;
	test(strcmp(strVal(linitial(fc->funcname)), "count") == 0);
	test(fc->agg_star == true);
	expect_int_const(e->rexpr, 1);
}

static void test_tree_select_qualified_name(void)
{
	SelectStmt *s = parse_select_one("SELECT * FROM schema.table");

	RangeVar *rv = linitial(s->fromClause);
	test(IsA(rv, RangeVar));
	test(rv->catalogname == NULL);
	test(strcmp(rv->schemaname, "schema") == 0);
	test(strcmp(rv->relname, "table") == 0);

	s = parse_select_one("SELECT * FROM db.schema.table");
	rv = linitial(s->fromClause);
	test(strcmp(rv->catalogname, "db") == 0);
	test(strcmp(rv->schemaname, "schema") == 0);
	test(strcmp(rv->relname, "table") == 0);
}

static void test_tree_select_alias(void)
{
	SelectStmt *s = parse_select_one("SELECT a AS x FROM t AS alias_t");

	ResTarget *rt = linitial(s->targetList);
	test(strcmp(rt->name, "x") == 0);
	expect_column_ref(rt->val, "a");

	RangeVar *rv = linitial(s->fromClause);
	test(rv->alias != NULL);
	test(strcmp(rv->alias->aliasname, "alias_t") == 0);
	test(rv->alias->colnames == NIL);
	test(strcmp(rv->relname, "t") == 0);

	s = parse_select_one("SELECT a x FROM t alias_t");
	rt = linitial(s->targetList);
	test(strcmp(rt->name, "x") == 0);
	rv = linitial(s->fromClause);
	test(strcmp(rv->alias->aliasname, "alias_t") == 0);
}

static void test_tree_select_typecast(void)
{
	SelectStmt *s = parse_select_one("SELECT 1::integer FROM t");

	ResTarget *rt = linitial(s->targetList);
	test(IsA(rt->val, TypeCast));
	TypeCast *tc = (TypeCast *)rt->val;
	expect_int_const(tc->arg, 1);
	test(IsA(tc->typeName, TypeName));
	test(list_length(tc->typeName->names) == 2);
	test(strcmp(strVal(linitial(tc->typeName->names)), "pg_catalog") == 0);
	test(strcmp(strVal(llast(tc->typeName->names)), "int4") == 0);
	test(tc->typeName->typmods == NIL);
	test(tc->typeName->arrayBounds == NIL);

	s = parse_select_one("SELECT '2024-01-01'::date FROM t");
	tc = (TypeCast *)((ResTarget *)linitial(s->targetList))->val;
	expect_string_const(tc->arg, "2024-01-01");
	test(strcmp(strVal(llast(tc->typeName->names)), "date") == 0);
}

static void test_tree_select_order_by(void)
{
	SelectStmt *s =
		parse_select_one("SELECT * FROM t ORDER BY a DESC, b ASC");

	test(list_length(s->sortClause) == 2);

	SortBy *sb = linitial(s->sortClause);
	test(IsA(sb, SortBy));
	test(sb->sortby_dir == SORTBY_DESC);
	test(sb->sortby_nulls == SORTBY_NULLS_DEFAULT);
	expect_column_ref(sb->node, "a");

	sb = lsecond(s->sortClause);
	test(sb->sortby_dir == SORTBY_ASC);
	expect_column_ref(sb->node, "b");
}

static void test_tree_select_limit_offset(void)
{
	SelectStmt *s = parse_select_one("SELECT * FROM t LIMIT 10 OFFSET 5");

	expect_int_const(s->limitCount, 10);
	expect_int_const(s->limitOffset, 5);
}

static void test_tree_select_distinct(void)
{
	SelectStmt *s = parse_select_one("SELECT DISTINCT a, b FROM t");

	test(s->distinctClause != NULL);
	test(list_length(s->distinctClause) == 1);
	test(list_length(s->targetList) == 2);
}

static void test_tree_select_union(void)
{
	SelectStmt *s =
		parse_select_one("SELECT * FROM a UNION SELECT * FROM b");

	test(s->op == SETOP_UNION);
	test(s->all == false);
	test(s->larg != NULL);
	test(s->rarg != NULL);
	test(IsA(s->larg, SelectStmt));
	test(IsA(s->rarg, SelectStmt));

	SelectStmt *l = (SelectStmt *)s->larg;
	SelectStmt *r = (SelectStmt *)s->rarg;
	test(l->op == SETOP_NONE && r->op == SETOP_NONE);
	test(l->larg == NULL && l->rarg == NULL);
	test(list_length(l->targetList) == 1);
	test(list_length(r->targetList) == 1);
	test(strcmp(((RangeVar *)linitial(l->fromClause))->relname, "a") == 0);
	test(strcmp(((RangeVar *)linitial(r->fromClause))->relname, "b") == 0);
}

static void test_tree_select_subquery(void)
{
	SelectStmt *s =
		parse_select_one("SELECT * FROM (SELECT * FROM t) AS sub");

	test(list_length(s->fromClause) == 1);
	RangeSubselect *rs = linitial(s->fromClause);
	test(IsA(rs, RangeSubselect));
	test(rs->lateral == false);
	test(rs->subquery != NULL);
	test(IsA(rs->subquery, SelectStmt));
	test(rs->alias != NULL);
	test(strcmp(rs->alias->aliasname, "sub") == 0);

	SelectStmt *inner = (SelectStmt *)rs->subquery;
	test(inner->op == SETOP_NONE);
	test(list_length(inner->targetList) == 1);
	test(list_length(inner->fromClause) == 1);
}

static void test_tree_select_join(void)
{
	SelectStmt *s = parse_select_one("SELECT * FROM a JOIN b ON a.x = b.y");

	test(list_length(s->fromClause) == 1);
	JoinExpr *j = linitial(s->fromClause);
	test(IsA(j, JoinExpr));
	test(j->jointype == JOIN_INNER);
	test(j->isNatural == false);
	test(j->usingClause == NIL);
	test(j->larg != NULL);
	test(j->rarg != NULL);

	RangeVar *lv = (RangeVar *)j->larg;
	test(IsA(lv, RangeVar));
	test(strcmp(lv->relname, "a") == 0);
	RangeVar *rv = (RangeVar *)j->rarg;
	test(IsA(rv, RangeVar));
	test(strcmp(rv->relname, "b") == 0);

	expect_simple_a_expr(j->quals, "=");
	A_Expr *e = (A_Expr *)j->quals;
	ColumnRef *lc = (ColumnRef *)e->lexpr;
	test(IsA(lc, ColumnRef));
	test(list_length(lc->fields) == 2);
	test(strcmp(strVal(linitial(lc->fields)), "a") == 0);
	test(strcmp(strVal(llast(lc->fields)), "x") == 0);
	ColumnRef *rc = (ColumnRef *)e->rexpr;
	test(strcmp(strVal(linitial(rc->fields)), "b") == 0);
	test(strcmp(strVal(llast(rc->fields)), "y") == 0);
}

static void test_tree_select_left_join(void)
{
	SelectStmt *s =
		parse_select_one("SELECT * FROM a LEFT JOIN b ON a.x = b.y");

	JoinExpr *j = linitial(s->fromClause);
	test(j->jointype == JOIN_LEFT);
	expect_simple_a_expr(j->quals, "=");
}

/*
 * The parser must survive arena exhaustion: report the failure through
 * the error callback and return NULL instead of terminating the process.
 */
static void test_oom(void)
{
	static char big[8192];
	static char query[8192];

	/*
	 * Case 1: the query text itself does not fit into the arena, the
	 * scan buffer allocation fails immediately.
	 */
	static _Alignas(max_align_t) uint8_t tiny_buf[1024];
	od_linear_alloc_t tiny_arena;
	od_linear_alloc_init(&tiny_arena, tiny_buf, sizeof(tiny_buf));

	size_t off = snprintf(big, sizeof(big), "%s", "SELECT ");
	for (int i = 0; off < sizeof(big) - 64; i++) {
		off += snprintf(big + off, sizeof(big) - off, "a%d, ", i);
	}
	snprintf(big + off, sizeof(big) - off, "%s", "FROM t");

	s_err_buf[0] = '\0';
	List *result = od_sql_full_parse(&tiny_arena, big, strlen(big),
					 on_error, NULL);
	test(result == NULL);
	test(s_err_buf[0] != '\0');

	/*
	 * Case 2: the query text fits into the arena, but the resulting
	 * parse tree does not.  The arena must be big enough to hold the
	 * flex DFA state buffer (~64KB), which is allocated regardless of
	 * the input size.
	 */
	static _Alignas(max_align_t) uint8_t mid_buf[128 * 1024];
	od_linear_alloc_t mid_arena;
	od_linear_alloc_init(&mid_arena, mid_buf, sizeof(mid_buf));

	off = snprintf(query, sizeof(query), "%s", "SELECT ");
	for (int i = 0; off < sizeof(query) - 64; i++) {
		off += snprintf(query + off, sizeof(query) - off, "column_%d, ",
				i);
	}
	/* strip the trailing ", " */
	off -= 2;
	snprintf(query + off, sizeof(query) - off, "%s", " FROM t");

	s_err_buf[0] = '\0';
	result = od_sql_full_parse(&mid_arena, query, strlen(query), on_error,
				   NULL);
	test(result == NULL);
	test(s_err_buf[0] != '\0');

	/* the parser must remain fully usable after OOM */
	s_err_buf[0] = '\0';
	List *ok = od_sql_full_parse(&mid_arena, "SELECT * FROM t",
				     strlen("SELECT * FROM t"), on_error, NULL);
	test(ok != NULL);
	test(s_err_buf[0] == '\0');
	test(list_length(ok) == 1);
}

#define OD_TEST_FULL_PARSE_THREADS 4
#define OD_TEST_FULL_PARSE_ITERS 200

struct full_parse_thread_arg {
	_Alignas(max_align_t) uint8_t arena_buf[OD_TEST_FULL_PARSE_ARENA_SIZE];
	od_linear_alloc_t arena;
	char err_buf[1024];
	int failures;
};

static void threaded_on_error(const char *msg, void *userdata)
{
	struct full_parse_thread_arg *arg = userdata;

	strncpy(arg->err_buf, msg, sizeof(arg->err_buf) - 1);
	arg->err_buf[sizeof(arg->err_buf) - 1] = '\0';
}

static void *full_parse_thread(void *argp)
{
	struct full_parse_thread_arg *arg = argp;

	od_linear_alloc_init(&arg->arena, arg->arena_buf,
			     sizeof(arg->arena_buf));

	for (int i = 0; i < OD_TEST_FULL_PARSE_ITERS; i++) {
		arg->err_buf[0] = '\0';
		List *ok =
			od_sql_full_parse(&arg->arena,
					  "SELECT * FROM t WHERE a = 1",
					  strlen("SELECT * FROM t WHERE a = 1"),
					  threaded_on_error, arg);
		if (ok == NULL || arg->err_buf[0] != '\0') {
			arg->failures++;
			continue;
		}

		arg->err_buf[0] = '\0';
		List *bad = od_sql_full_parse(&arg->arena, "SELECT FROM",
					      strlen("SELECT FROM"),
					      threaded_on_error, arg);
		if (bad != NULL || arg->err_buf[0] == '\0') {
			arg->failures++;
		}
	}
	return NULL;
}

static void test_concurrent_parse(void)
{
	static pthread_t threads[OD_TEST_FULL_PARSE_THREADS];
	static struct full_parse_thread_arg args[OD_TEST_FULL_PARSE_THREADS];

	memset(args, 0, sizeof(args));

	for (int i = 0; i < OD_TEST_FULL_PARSE_THREADS; i++) {
		int rc = pthread_create(&threads[i], NULL, full_parse_thread,
					&args[i]);
		test(rc == 0);
	}

	for (int i = 0; i < OD_TEST_FULL_PARSE_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	for (int i = 0; i < OD_TEST_FULL_PARSE_THREADS; i++) {
		test(args[i].failures == 0);
	}
}

void odyssey_test_sql_full_parser(void)
{
	od_linear_alloc_init(&s_arena, s_arena_buf, sizeof(s_arena_buf));

	test_select_star();
	test_select_columns();
	test_select_where();
	test_select_join();
	test_select_left_join();
	test_select_subquery();
	test_select_order_by();
	test_select_limit_offset();
	test_select_group_having();
	test_select_union();
	test_select_distinct();
	test_select_constants();
	test_select_qualified_name();
	test_select_alias();
	test_select_expr();
	test_select_case_insensitive();
	test_select_comments();
	test_select_string_escape();
	test_select_dollar_quote();
	test_select_typecast();
	test_parse_error();

	test_tree_select_star();
	test_tree_select_columns();
	test_tree_select_where();
	test_tree_select_bool_and_null();
	test_tree_select_constants();
	test_tree_select_func_call();
	test_tree_select_having();
	test_tree_select_qualified_name();
	test_tree_select_alias();
	test_tree_select_typecast();
	test_tree_select_order_by();
	test_tree_select_limit_offset();
	test_tree_select_distinct();
	test_tree_select_union();
	test_tree_select_subquery();
	test_tree_select_join();
	test_tree_select_left_join();
	test_oom();

	test_concurrent_parse();
}
