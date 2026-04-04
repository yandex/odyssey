#include <machinarium/machinarium.h>

#include <odyssey.h>
#include <client.h>
#include <server.h>
#include <tests/odyssey_test.h>

#include <pstmt.h>

void test_pstmt_client_hashmap(void)
{
	od_client_t *client = od_client_allocate();
	client->prep_stmt_ids = od_client_pstmt_hashmap_create();
	test(client->prep_stmt_ids != NULL);

	od_pstmt_t pstmt;
	memset(&pstmt, 0, sizeof(pstmt));

	od_pstmt_t pstmt2;
	memset(&pstmt2, 0, sizeof(pstmt2));

	/* works correctly with unnamed */
	test(od_client_has_pstmt(client, "") == 0);
	test(od_client_get_pstmt(client, "") == NULL);

	test(od_client_add_pstmt(client, "", &pstmt) == 0);
	test(od_client_has_pstmt(client, "") == 1);
	test(od_client_get_pstmt(client, "") == &pstmt);

	/* allows to redefine unnamed statement */
	test(od_client_add_pstmt(client, "", &pstmt2) == 0);
	test(od_client_has_pstmt(client, "") == 1);
	test(od_client_get_pstmt(client, "") == &pstmt2);

	test(od_client_remove_pstmt(client, "") == 0);
	test(od_client_has_pstmt(client, "") == 0);
	test(od_client_get_pstmt(client, "") == NULL);

	test(od_client_has_pstmt(client, "P_0") == 0);
	test(od_client_get_pstmt(client, "P_0") == NULL);

	test(od_client_has_pstmt(client, "P_0") == 0);
	test(od_client_get_pstmt(client, "P_0") == NULL);

	test(od_client_add_pstmt(client, "P_0", &pstmt) == 0);
	test(od_client_has_pstmt(client, "P_0") == 1);
	test(od_client_get_pstmt(client, "P_0") == &pstmt);
	/* do not add twice */
	test(od_client_add_pstmt(client, "P_0", &pstmt) == 1);
	test(od_client_has_pstmt(client, "P_0") == 1);
	test(od_client_get_pstmt(client, "P_0") == &pstmt);

	test(od_client_remove_pstmt(client, "P_1") == 0);
	test(od_client_has_pstmt(client, "P_1") == 0);
	test(od_client_get_pstmt(client, "P_1") == NULL);

	test(od_client_add_pstmt(client, "P_1", &pstmt2) == 0);
	test(od_client_has_pstmt(client, "P_1") == 1);
	test(od_client_get_pstmt(client, "P_1") == &pstmt2);

	test(od_client_remove_pstmt(client, "P_0") == 0);
	test(od_client_has_pstmt(client, "P_1") == 1);
	test(od_client_get_pstmt(client, "P_1") == &pstmt2);
	test(od_client_has_pstmt(client, "P_0") == 0);
	test(od_client_get_pstmt(client, "P_0") == NULL);

	od_client_pstmts_clear(client);
	test(od_client_has_pstmt(client, "P_1") == 0);
	test(od_client_get_pstmt(client, "P_1") == NULL);

	test(od_client_add_pstmt(client, "dangling", &pstmt) == 0);
	test(od_client_has_pstmt(client, "dangling") == 1);
	test(od_client_get_pstmt(client, "dangling") == &pstmt);

	od_client_free(client);
}

static void test_pstmt_server_hashmap(void)
{
	od_server_t *server = od_server_allocate(1);

	od_pstmt_t pstmt1;
	od_pstmt_next_name(&pstmt1);
	pstmt1.desc.data = "data1";
	pstmt1.desc.len = sizeof("data1");

	od_pstmt_t pstmt2;
	od_pstmt_next_name(&pstmt2);
	pstmt2.desc.data = "data2";
	pstmt2.desc.len = sizeof("data2");

	test(od_server_has_pstmt(server, &pstmt1) == 0);
	test(od_server_has_pstmt(server, &pstmt2) == 0);

	test(od_server_remove_pstmt(server, &pstmt1) == 0);
	test(od_server_remove_pstmt(server, &pstmt2) == 0);

	test(od_server_add_pstmt(server, &pstmt1) == 0);
	test(od_server_has_pstmt(server, &pstmt1) == 1);
	test(od_server_has_pstmt(server, &pstmt2) == 0);

	test(od_server_add_pstmt(server, &pstmt2) == 0);
	test(od_server_has_pstmt(server, &pstmt1) == 1);
	test(od_server_has_pstmt(server, &pstmt2) == 1);

	test(od_server_remove_pstmt(server, &pstmt2) == 0);
	test(od_server_has_pstmt(server, &pstmt1) == 1);
	test(od_server_has_pstmt(server, &pstmt2) == 0);

	od_server_pstmts_clear(server);
	test(od_server_has_pstmt(server, &pstmt1) == 0);
	test(od_server_has_pstmt(server, &pstmt2) == 0);

	od_pstmt_t dangling;
	od_pstmt_next_name(&dangling);
	dangling.desc.data = "dangling";
	dangling.desc.len = sizeof("dangling");

	od_pstmt_t danglingcopy;
	memcpy(&danglingcopy, &dangling, sizeof(od_pstmt_t));
	test(od_server_add_pstmt(server, &dangling) == 0);
	test(od_server_has_pstmt(server, &dangling) == 1);
	test(od_server_has_pstmt(server, &danglingcopy) == 1);

	od_server_free(server);
}

static void test_pstmt_global(void)
{
	mm_hashmap_t *gm = od_global_pstmts_map_create();

	od_pstmt_desc_t desc;
	desc.data = "data";
	desc.len = sizeof("data");

	od_pstmt_t *pstmt = od_pstmt_create_or_get(gm, desc);
	test(pstmt != NULL);
	test(desc.len == pstmt->desc.len);
	test(memcmp(desc.data, pstmt->desc.data, pstmt->desc.len) == 0);
	test(&pstmt->desc != &desc);

	test(od_pstmt_create_or_get(gm, desc) == pstmt);

	od_global_pstmts_map_free(gm);
}

static void test_impl(void *a)
{
	(void)a;

	test_pstmt_client_hashmap();
	test_pstmt_server_hashmap();
	test_pstmt_global();
}

void odyssey_test_pstmt(void)
{
	machinarium_init();

	int64_t rc;
	rc = machine_create("test_impl", test_impl, NULL);
	test(rc > 0);

	test(machine_wait(rc) == 0);

	machinarium_free();
}
