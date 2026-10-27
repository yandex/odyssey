#include <odyssey.h>
#include <tests/odyssey_test.h>
#include <string.h>

#include <sql_pstmt.h>
#include <alloc/linear.h>

#define OD_TEST_SQL_PSTMT_ARENA_SIZE (1024 * 1024)

static _Alignas(max_align_t) uint8_t s_arena_buf[OD_TEST_SQL_PSTMT_ARENA_SIZE];
static od_linear_alloc_t s_arena;

static void analyze(od_sql_pstmt_query_t *q, const char *query)
{
	memset(q, 0, sizeof(od_sql_pstmt_query_t));
	od_linear_alloc_init(&s_arena, s_arena_buf, sizeof(s_arena_buf));
	test(od_sql_pstmt_query_analyze(q, query, strlen(query), &s_arena) ==
	     0);
	test(q->analyzed);
}

static void test_no_keyword(void)
{
	od_sql_pstmt_query_t q;
	analyze(&q, "EXECUTE foo(1); EXECUTE foo(2)");
	test(q.nops == 0);
	od_sql_pstmt_query_reset(&q);
}

static void test_keyword_in_literal(void)
{
	od_sql_pstmt_query_t q;
	analyze(&q, "SELECT 'prepare deallocate'");
	test(q.nops == 0);
	od_sql_pstmt_query_reset(&q);
}

static void test_single_prepare(void)
{
	const char *query = "PREPARE foo AS SELECT $1::int";
	od_sql_pstmt_query_t q;
	analyze(&q, query);
	test(q.nops == 1);
	test(q.ops[0].type == OD_SQL_PSTMT_OP_PREPARE);
	test(q.ops[0].stmt_idx == 0);
	test(strcmp(q.ops[0].name, "foo") == 0);
	test(strcmp(q.ops[0].text, query) == 0);
	test(q.ops[0].text_len == strlen(query));
	od_sql_pstmt_query_reset(&q);
}

static void test_prepare_with_types_and_case(void)
{
	od_sql_pstmt_query_t q;
	analyze(&q, "prepare FOO (int, text) as select $1, $2");
	test(q.nops == 1);
	/* unquoted names are folded to lower case, as PostgreSQL does */
	test(strcmp(q.ops[0].name, "foo") == 0);
	od_sql_pstmt_query_reset(&q);

	analyze(&q, "PREPARE \"Mixed\"\"Name\" AS SELECT 1");
	test(q.nops == 1);
	test(strcmp(q.ops[0].name, "Mixed\"Name") == 0);
	od_sql_pstmt_query_reset(&q);
}

static void test_prepare_with_comments(void)
{
	od_sql_pstmt_query_t q;
	analyze(&q, "/* app */ PREPARE foo AS SELECT 1 -- trailing");
	test(q.nops == 1);
	test(strcmp(q.ops[0].name, "foo") == 0);
	od_sql_pstmt_query_reset(&q);
}

static void test_multi_statement(void)
{
	const char *query =
		"BEGIN; PREPARE a AS SELECT 1; SELECT 2; PREPARE b AS SELECT 3; EXECUTE a";
	od_sql_pstmt_query_t q;
	analyze(&q, query);
	test(q.nops == 2);

	test(q.ops[0].stmt_idx == 1);
	test(strcmp(q.ops[0].name, "a") == 0);
	test(strstr(q.ops[0].text, "PREPARE a AS SELECT 1") != NULL);
	test(strchr(q.ops[0].text, ';') == NULL);

	test(q.ops[1].stmt_idx == 3);
	test(strcmp(q.ops[1].name, "b") == 0);
	test(strstr(q.ops[1].text, "PREPARE b AS SELECT 3") != NULL);
	test(strchr(q.ops[1].text, ';') == NULL);

	/* the same statement text gives the same hash wherever it appears */
	od_sql_pstmt_query_t single;
	uint64_t hash_b = q.ops[1].hash;
	od_sql_pstmt_query_reset(&q);

	analyze(&single, "PREPARE b AS SELECT 3");
	test(single.ops[0].hash == hash_b);
	od_sql_pstmt_query_reset(&single);
}

static void test_deallocate(void)
{
	od_sql_pstmt_query_t q;

	analyze(&q, "DEALLOCATE foo");
	test(q.nops == 1);
	test(q.ops[0].type == OD_SQL_PSTMT_OP_DEALLOCATE);
	test(strcmp(q.ops[0].name, "foo") == 0);
	od_sql_pstmt_query_reset(&q);

	analyze(&q, "DEALLOCATE PREPARE \"Foo\"");
	test(q.nops == 1);
	test(q.ops[0].type == OD_SQL_PSTMT_OP_DEALLOCATE);
	test(strcmp(q.ops[0].name, "Foo") == 0);
	od_sql_pstmt_query_reset(&q);

	analyze(&q, "DEALLOCATE ALL");
	test(q.nops == 1);
	test(q.ops[0].type == OD_SQL_PSTMT_OP_DEALLOCATE_ALL);
	test(q.ops[0].name == NULL);
	od_sql_pstmt_query_reset(&q);

	analyze(&q, "deallocate prepare all");
	test(q.nops == 1);
	test(q.ops[0].type == OD_SQL_PSTMT_OP_DEALLOCATE_ALL);
	od_sql_pstmt_query_reset(&q);
}

static void test_invalid_query(void)
{
	od_sql_pstmt_query_t q;
	analyze(&q, "PREPARE foo AS");
	test(q.nops == 0);
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

static void test_map(void)
{
	od_sql_pstmt_map_t map;
	od_sql_pstmt_map_init(&map);

	test(od_sql_pstmt_map_get(&map, "a") == NULL);
	od_sql_pstmt_map_remove(&map, "a");

	test(od_sql_pstmt_map_set(&map, "a", "PREPARE a AS SELECT 1", 21, 1) !=
	     NULL);
	test(od_sql_pstmt_map_set(&map, "b", NULL, 0, 2) != NULL);
	test(map.count == 2);

	od_sql_pstmt_t *a = od_sql_pstmt_map_get(&map, "a");
	test(a != NULL);
	test(a->hash == 1);
	test(strcmp(a->text, "PREPARE a AS SELECT 1") == 0);
	test(od_sql_pstmt_map_get(&map, "b")->text == NULL);

	/* replacing moves the entry to the end of the insertion order */
	test(od_sql_pstmt_map_set(&map, "a", NULL, 0, 3) != NULL);
	test(map.count == 2);
	test(od_sql_pstmt_map_get(&map, "a")->hash == 3);
	od_sql_pstmt_t *first =
		od_container_of(map.order.next, od_sql_pstmt_t, link);
	test(strcmp(first->name, "b") == 0);

	od_sql_pstmt_map_remove(&map, "b");
	test(map.count == 1);
	test(od_sql_pstmt_map_get(&map, "b") == NULL);

	od_sql_pstmt_map_clear(&map);
	test(map.count == 0);
	test(od_sql_pstmt_map_get(&map, "a") == NULL);

	test(od_sql_pstmt_map_set(&map, "c", NULL, 0, 4) != NULL);
	od_sql_pstmt_map_free(&map);
}

static void test_attach_refuses_unknown_server(void)
{
	od_sql_pstmt_client_t client;
	od_sql_pstmt_server_t srv;

	od_sql_pstmt_client_init(&client);
	od_sql_pstmt_server_init(&srv);
	srv.unknown = 1;

	/* refused before any io, so no server connection is needed */
	test(od_sql_pstmt_attach(&client, &srv, NULL, "test", 0) == 1);

	od_sql_pstmt_client_free(&client);
	od_sql_pstmt_server_free(&srv);
}

void odyssey_test_sql_pstmt(void)
{
	test_no_keyword();
	test_keyword_in_literal();
	test_single_prepare();
	test_prepare_with_types_and_case();
	test_prepare_with_comments();
	test_multi_statement();
	test_deallocate();
	test_invalid_query();
	test_reserved_names();
	test_map();
	test_attach_refuses_unknown_server();
}
