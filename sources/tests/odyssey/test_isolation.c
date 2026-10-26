#include <odyssey.h>

#include <isolation.h>
#include <pstmt.h>
#include <client.h>
#include <server.h>
#include <route.h>
#include <storage.h>
#include <global.h>
#include <instance.h>
#include <worker.h>
#include <tests/odyssey_test.h>

static _Alignas(max_align_t) uint8_t arena_buffer[5 * 1024 * 1024];
static od_config_query_parsing_t parsing = {
	.mode = OD_CONFIG_QUERY_PARSING_MODE_MINIMAL,
	.mem_limit_bytes = sizeof(arena_buffer),
	.max_query_len = 10000,
};

static od_pstmt_t *prepare(od_global_pstmt_map_t *map, const char *query)
{
	machine_msg_t *msg =
		kiwi_fe_write_parse(NULL, "probe", 6, (char *)query,
				    strlen(query) + 1, 1, (int[]){ 23 });
	test(msg != NULL);
	od_pstmt_t *pstmt =
		od_pstmt_create_or_get(map, od_pstmt_desc_from_parse(msg),
				       od_worker_get_local_linear_alloc(),
				       &parsing);
	test(pstmt != NULL);
	machine_msg_free(msg);
	return pstmt;
}

static void test_probe_recognition(od_global_pstmt_map_t *map)
{
	struct {
		const char *sql;
		int matches;
	} cases[] = {
		{ "SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{10,20}')",
		  1 },
		{ "SELECT PG_CATALOG.PG_ISOLATION_TEST_SESSION_IS_BLOCKED($1, '{10,20}') AS blocked",
		  1 },
		{ "SELECT /* probe */ pg_catalog.pg_isolation_test_session_is_blocked($1, '{}')",
		  1 },
		{ "SELECT 'pg_isolation_test_session_is_blocked'", 0 },
		{ "SELECT public.pg_isolation_test_session_is_blocked($1, '{10,20}')",
		  0 },
		{ "SELECT pg_catalog.pg_isolation_test_session_is_blocked($2, '{10,20}')",
		  0 },
		{ "SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{10,NULL}')",
		  0 },
		{ "SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{2147483648}')",
		  0 },
		{ "SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{10,20}'), $1",
		  0 },
		{ "SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{10,20}') WHERE $1 = 10",
		  0 },
		{ "SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{10,20}'); SELECT 1",
		  0 },
		{ "SELECT pg_catalog.pg_isolation_test_session_is_blocked(",
		  0 },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		od_pstmt_t *pstmt = prepare(map, cases[i].sql);
		test((pstmt->isolation_pids_len != 0) == cases[i].matches);
		od_pstmt_unref(pstmt);
	}
}

static void check_bind(od_client_t *client, od_pstmt_t *original,
		       uint32_t victim, int binary, const char *expected_query,
		       const char *expected_pid)
{
	char text[16];
	int len = snprintf(text, sizeof(text), "%u", victim);
	if (binary) {
		kiwi_write32to(text, victim);
		len = 4;
	}
	machine_msg_t *msg =
		kiwi_fe_write_bind(NULL, "portal", 7, "probe", 6, 1, &binary, 1,
				   (int[]){ 1 }, 1, &len, (char *[]){ text });
	test(msg != NULL);
	od_pstmt_t *rewritten = NULL;
	machine_msg_t *bind = NULL;
	test(od_isolation_probe_bind(client, original, msg, &rewritten,
				     &bind) == OD_OK);
	test(rewritten != NULL && bind != NULL);
	test(strcmp(rewritten->desc.data, expected_query) == 0);
	/* The Parse's declared parameter type remains int4. */
	size_t query_len = strlen(rewritten->desc.data) + 1;
	test(rewritten->desc.len == query_len + 6);
	test(memcmp((char *)rewritten->desc.data + query_len, "\0\1\0\0\0\27",
		    6) == 0);

	char *p = machine_msg_data(bind);
	uint32_t rem = machine_msg_size(bind);
	test(kiwi_readn(sizeof(kiwi_header_t), &p, &rem) == 0);
	test(strcmp(p, "portal") == 0);
	test(kiwi_readsz(&p, &rem) == 0);
	test(strcmp(p, rewritten->name) == 0);
	test(kiwi_readsz(&p, &rem) == 0);
	uint16_t count;
	test(kiwi_read16(&count, &p, &rem) == 0 && count == 0);
	test(kiwi_read16(&count, &p, &rem) == 0 && count == 1);
	uint32_t size;
	test(kiwi_read32(&size, &p, &rem) == 0 && size == strlen(expected_pid));
	test(rem >= size && memcmp(p, expected_pid, size) == 0);
	test(kiwi_readn(size, &p, &rem) == 0);
	test(kiwi_read16(&count, &p, &rem) == 0 && count == 1);
	test(kiwi_read16(&count, &p, &rem) == 0 && count == 1);
	test(rem == 0);
	machine_msg_free(bind);
	machine_msg_free(msg);
	od_pstmt_unref(rewritten);
}

static void test_isolation(void *arg)
{
	(void)arg;
	od_linear_alloc_init(od_worker_get_local_linear_alloc(), arena_buffer,
			     sizeof(arena_buffer));
	od_global_pstmt_map_t *map = od_global_pstmts_map_create(1);
	test(map != NULL);
	test_probe_recognition(map);

	od_instance_t instance = { 0 };
	instance.pstmts = map;
	instance.config.query_parsing = parsing;
	od_global_t global = { .instance = &instance };
	od_route_t route = { 0 };
	od_multi_pool_t pool = { 0 };
	route.exclusive_pool = &pool;
	mm_mutex_init(&route.lock);
	od_client_pool_init(&route.client_pool);
	od_storage_endpoint_t endpoint = { 0 }, other_endpoint = { 0 };
	od_server_t server = { .endpoint = &endpoint };
	atomic_init(&server.backend_pid, 100);
	od_client_t peer = { 0 };
	peer.server = &server;
	peer.key.key_pid = 10;
	od_list_init(&peer.link_pool);
	od_list_append(&route.client_pool.active, &peer.link_pool);
	od_client_t control = { .route = &route,
				.server = &server,
				.global = &global };

	od_pstmt_t *pstmt = prepare(
		map,
		"SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{10,20}')");
	check_bind(
		&control, pstmt, 10, 0,
		"SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{100}')",
		"100");
	/* The same prepared probe must follow a changed backend assignment. */
	atomic_store(&server.backend_pid, 200);
	check_bind(
		&control, pstmt, 10, 1,
		"SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{200}')",
		"200");
	check_bind(
		&control, pstmt, 99, 0,
		"SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{200}')",
		"0");
	od_server_t control_server = { .endpoint = &other_endpoint };
	control.server = &control_server;
	check_bind(
		&control, pstmt, 10, 0,
		"SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{}')",
		"0");
	control.server = &server;
	od_list_unlink(&peer.link_pool);
	check_bind(
		&control, pstmt, 10, 0,
		"SELECT pg_catalog.pg_isolation_test_session_is_blocked($1, '{}')",
		"0");
	od_pstmt_unref(pstmt);
	mm_mutex_destroy(&route.lock);
	od_global_pstmts_map_free(map);
}

void odyssey_test_isolation(void)
{
	test(machinarium_init() == 0);
	int id = machine_create("isolation", test_isolation, NULL);
	test(id != -1);
	test(machine_wait(id) == 0);
	machinarium_free();
}
