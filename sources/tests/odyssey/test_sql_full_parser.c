#include <odyssey.h>
#include <tests/odyssey_test.h>
#include <pthread.h>
#include <string.h>

#include <sql/full/parser.h>
#include <sql/full/ast.h>

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
	List *result = od_sql_full_parse(input, strlen(input), on_error, NULL);
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
	List *result = od_sql_full_parse(input, strlen(input), on_error, NULL);
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

#define OD_TEST_FULL_PARSE_THREADS 4
#define OD_TEST_FULL_PARSE_ITERS 200

struct full_parse_thread_arg {
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

	for (int i = 0; i < OD_TEST_FULL_PARSE_ITERS; i++) {
		arg->err_buf[0] = '\0';
		List *ok =
			od_sql_full_parse("SELECT * FROM t WHERE a = 1",
					  strlen("SELECT * FROM t WHERE a = 1"),
					  threaded_on_error, arg);
		if (ok == NULL || arg->err_buf[0] != '\0') {
			arg->failures++;
			continue;
		}

		arg->err_buf[0] = '\0';
		List *bad =
			od_sql_full_parse("SELECT FROM", strlen("SELECT FROM"),
					  threaded_on_error, arg);
		if (bad != NULL || arg->err_buf[0] == '\0') {
			arg->failures++;
		}
	}
	return NULL;
}

static void test_concurrent_parse(void)
{
	pthread_t threads[OD_TEST_FULL_PARSE_THREADS];
	struct full_parse_thread_arg args[OD_TEST_FULL_PARSE_THREADS];

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
	test_concurrent_parse();
}
