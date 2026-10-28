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
		"EXECUTE name(1)",
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
		"EXECUTE p(1 + 2)",
		"EXECUTE p('1'::int)",
		"EXECUTE p(length('a'))",
		"EXECUTE p(ARRAY[1, 2])",
		"EXECUTE p(B'101')",
		"PREPARE \"na\"\"me\" AS SELECT 1",
		"PREPARE U&\"na\\006de\" AS SELECT 1",
		"EXECUTE U&\"na!006de\" UESCAPE '!'(1)",
		"DEALLOCATE U&\"na\\006de\"",
		"EXPLAIN EXECUTE name(1)",
		"CREATE TEMP TABLE result AS EXECUTE name(1)",
		"PREPARE ok AS SELECT 1; EXECUTE U&\"na\\006de\"",
		"PREPARE abcdefghijklmnopqrstuvwxyz_abcdefghijklmnopqrstuvwxyz_0123456789_x AS SELECT 1",
	};
	for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
		od_sql_pstmt_query_t q;
		analyze(&q, inputs[i]);
		test(q.unsupported && !q.uses_reserved_name);
		od_sql_pstmt_query_reset(&q);
	}
}

static void test_execute_arguments(void)
{
	od_sql_pstmt_query_t q;
	analyze(&q,
		"EXECUTE p(-42, 2147483648, -12.34, 'a''b', E'a\\nb', true, false, NULL, $$a;b$$)");
	test(q.active && !q.unsupported && q.nops == 1);
	od_sql_pstmt_op_t *op = &q.ops[0];
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

static void test_reserved_names(void)
{
	static const char *rejected[] = {
		"PREPARE odyssey_pstmt_1 AS SELECT 1",
		"prepare \"odyssey_pstmt_x\" as select 1",
		"EXECUTE odyssey_pstmt_7",
		"execute ODYSSEY_PSTMT_7(1, 2)",
		"DEALLOCATE odyssey_pstmt_3",
		"DEALLOCATE PREPARE odyssey_pstmt_3",
		"EXPLAIN EXECUTE odyssey_pstmt_2",
		"CREATE TABLE t AS EXECUTE odyssey_pstmt_2",
		"EXPLAIN ANALYZE CREATE TABLE t AS EXECUTE odyssey_pstmt_2",
		("EXPLAIN (ANALYZE) CREATE TEMP TABLE t AS "
		 "EXECUTE U&\"odyssey\\005fpstmt_2\""),
		"SELECT 1; EXECUTE odyssey_pstmt_9; SELECT 2",
		/* unicode escapes in identifiers */
		"EXECUTE U&\"odyssey\\005fpstmt_7\"",
		"execute u&\"\\006fdyssey_pstmt_7\"(1)",
		"EXECUTE U&\"odyssey!005fpstmt_7\" UESCAPE '!'",
		"DEALLOCATE U&\"odyssey\\005fpstmt_7\"",
		"PREPARE U&\"odyssey\\+00005fpstmt_7\" AS SELECT 1",
	};
	static const char *allowed[] = {
		/* quoted names are case-sensitive, this one is not odyssey's */
		"EXECUTE \"ODYSSEY_PSTMT_7\"",
		"SELECT 'odyssey_pstmt_1'",
		("SELECT name FROM pg_prepared_statements "
		 "WHERE name LIKE 'odyssey_pstmt_%'"),
		"PREPARE odyssey_pstm AS SELECT 1",
		"EXECUTE my_odyssey_pstmt_1",
	};
	od_sql_pstmt_query_t q;

	for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
		analyze(&q, rejected[i]);
		test(q.uses_reserved_name);
		test(q.nops == 0);
		od_sql_pstmt_query_reset(&q);
	}

	for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); ++i) {
		analyze(&q, allowed[i]);
		test(!q.uses_reserved_name);
		od_sql_pstmt_query_reset(&q);
	}
}

void odyssey_test_sql_pstmt(void)
{
	test_locations();
	test_identifier_forms();
	test_unsupported_syntax();
	test_execute_arguments();
	test_reserved_names();
}
