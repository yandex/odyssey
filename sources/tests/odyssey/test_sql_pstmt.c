#include <odyssey.h>
#include <tests/odyssey_test.h>
#include <sql_pstmt.h>
#include <alloc/linear.h>

static _Alignas(max_align_t) uint8_t buffer[1024 * 1024];
static od_linear_alloc_t arena;

static void analyze(od_sql_pstmt_query_t *q, const char *query)
{
	memset(q, 0, sizeof(*q));
	od_linear_alloc_init(&arena, buffer, sizeof(buffer));
	test(od_sql_pstmt_query_analyze(q, query, strlen(query), &arena) == 0);
	test(q->analyzed);
}

static void test_locations(void)
{
	const char *sql =
		"SELECT ';'; PREPARE \"Ab\" AS SELECT $1::numeric, $2::text[]; EXECUTE \"Ab\"(1, 'a;b'); DEALLOCATE ALL";
	od_sql_pstmt_query_t q;
	analyze(&q, sql);
	test(q.active);
	test(q.nops == 4);
	test(q.ops[0].type == OD_SQL_PSTMT_OTHER);
	test(q.ops[1].type == OD_SQL_PSTMT_PREPARE);
	test(strcmp(q.ops[1].name, "Ab") == 0);
	test(q.ops[1].query_location.start == strstr(sql, "SELECT $1") - sql);
	test(q.ops[1].query_location.end - q.ops[1].query_location.start ==
	     (int)strlen("SELECT $1::numeric, $2::text[]"));
	test(q.ops[2].type == OD_SQL_PSTMT_EXECUTE);
	test(strcmp(q.ops[2].name, q.ops[1].name) == 0);
	test(q.ops[3].type == OD_SQL_PSTMT_DEALLOCATE_ALL);
	od_sql_pstmt_query_reset(&q);
}

static void test_identifier_forms(void)
{
	const char *inputs[] = {
		"PREPARE prepare AS SELECT 'a; b'",
		"PREPARE \"name\" AS SELECT 'a; b'",
		"PREPARE name AS SELECT $1::numeric, $2::text[]",
	};
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		od_sql_pstmt_query_t q;
		analyze(&q, inputs[i]);
		test(q.nops == 1 && q.active && !q.unsupported);
		test(strcmp(q.ops[0].name, i == 0 ? "prepare" : "name") == 0);
		test(q.ops[0].name_location.start >= 0);
		od_sql_pstmt_query_reset(&q);
	}
}

static void test_unsupported_syntax(void)
{
	const char *inputs[] = {
		"PREPARE typed (int) AS SELECT $1",
		"PREPARE typed (numeric(10, 2), text[]) AS SELECT $1, $2",
		"SELECT 1; PREPARE typed (int) AS SELECT $1",
		"PREPARE p AS SELECT 1; COPY t FROM STDIN",
		"SELECT 1; EXECUTE p(1 + 2)",
		"SELECT 1; EXECUTE p('1'::int)",
		"SELECT 1; EXECUTE p(length('a'))",
		"SELECT 1; EXECUTE p(ARRAY[1, 2])",
		"SELECT 1; EXECUTE p(B'101')",
		"PREPARE \"na\"\"me\" AS SELECT 1",
		"PREPARE U&\"na\\006de\" AS SELECT 1",
		"SELECT 1; EXECUTE U&\"na!006de\" UESCAPE '!'(1)",
		"DEALLOCATE U&\"na\\006de\"",
		"EXPLAIN EXECUTE name(1)",
		"CREATE TEMP TABLE result AS EXECUTE name(1)",
		"PREPARE ok AS SELECT 1; EXECUTE U&\"na\\006de\"",
		"PREPARE abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_0123456789_x AS SELECT 1",
	};
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		od_sql_pstmt_query_t q;
		analyze(&q, inputs[i]);
		test(q.unsupported);
		od_sql_pstmt_query_reset(&q);
	}
}

static void test_execute_arguments(void)
{
	od_sql_pstmt_query_t q;
	analyze(&q,
		"SELECT 1; EXECUTE p(-42, 2147483648, -12.34, 'a''b', E'a\\nb', true, false, NULL, $$a;b$$)");
	test(q.active && !q.unsupported && q.nops == 2);
	od_sql_pstmt_op_t *op = &q.ops[1];
	test(op->nargs == 9);
	const char *values[] = { "-42",	 "2147483648", "-12.34", "a'b", "a\nb",
				 "true", "false",      NULL,	 "a;b" };
	memset(buffer, 0x5a, sizeof(buffer));
	for (int i = 0; i < op->nargs; i++) {
		if (values[i] == NULL) {
			test(op->args[i] == NULL && op->arg_lengths[i] == -1);
		} else {
			test(strcmp(op->args[i], values[i]) == 0);
			test(op->arg_lengths[i] == (int)strlen(values[i]));
		}
	}
	od_sql_pstmt_query_reset(&q);
}

void odyssey_test_sql_pstmt(void)
{
	test_locations();
	test_identifier_forms();
	test_unsupported_syntax();
	test_execute_arguments();
}
