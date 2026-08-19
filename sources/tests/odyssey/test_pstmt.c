#include <machinarium/machinarium.h>

#include <odyssey.h>
#include <client.h>
#include <server.h>
#include <tests/odyssey_test.h>

#include <pstmt.h>

static void test_refs(void)
{
	od_global_pstmt_map_t *global = od_global_pstmts_map_create(1);
	test(global != NULL);

	od_pstmt_desc_t desc;
	desc.data = "data1";
	desc.len = sizeof("data1");
	od_pstmt_t *ps = od_pstmt_create_or_get(global, desc);
	test(ps != NULL);

	od_pstmt_unref(ps);
	test(od_global_pstmts_has_pstmt(global, desc) == 0);

	ps = od_pstmt_create_or_get(global, desc);
	test(ps != NULL);

	od_pstmt_t *p2 = ps;
	od_pstmt_ref(ps);

	od_pstmt_unref(ps);
	test(od_global_pstmts_has_pstmt(global, desc) == 1);

	od_pstmt_unref(p2);
	test(od_global_pstmts_has_pstmt(global, desc) == 0);

	od_global_pstmts_map_free(global);
}

static void test_pstmt_global(void)
{
	od_global_pstmt_map_t *gm = od_global_pstmts_map_create(1);

	od_pstmt_desc_t desc;
	desc.data = "data";
	desc.len = sizeof("data");

	od_pstmt_t *pstmt = od_pstmt_create_or_get(gm, desc);
	test(pstmt != NULL);
	test(desc.len == pstmt->desc.len);
	test(memcmp(desc.data, pstmt->desc.data, pstmt->desc.len) == 0);
	test(&pstmt->desc != &desc);
	test(pstmt->desc.data != desc.data);

	od_pstmt_t *t = od_pstmt_create_or_get(gm, desc);
	test(t == pstmt);
	od_pstmt_unref(t);

	od_pstmt_t *pstmt2 = od_pstmt_create_or_get(gm, desc);
	test(pstmt2 == pstmt);
	test(pstmt2->desc.data == pstmt->desc.data);

	od_pstmt_desc_t desc_copy;
	desc_copy.data = od_strdup("data");
	test(desc.data != NULL);
	desc_copy.len = sizeof("data");
	od_pstmt_t *pstmt3 = od_pstmt_create_or_get(gm, desc_copy);
	test(pstmt3 == pstmt2);
	test(pstmt3 == pstmt);

	od_pstmt_unref(pstmt);
	od_pstmt_unref(pstmt2);
	od_pstmt_unref(pstmt3);

	od_free(desc_copy.data);

	test(od_global_pstmts_has_pstmt(gm, desc) == 0);

	od_global_pstmts_map_free(gm);
}

void test_pstmt_client_hashmap(void)
{
	od_global_pstmt_map_t *global = od_global_pstmts_map_create(1);
	test(global != NULL);

	od_client_t *client = od_client_allocate();
	client->prep_stmt_ids = od_client_pstmt_hashmap_create();
	test(client->prep_stmt_ids != NULL);

	od_pstmt_desc_t desc1;
	desc1.data = "data";
	desc1.len = sizeof("data");

	od_pstmt_desc_t desc2;
	desc2.data = "data2";
	desc2.len = sizeof("data2");

	od_pstmt_desc_t desc3;
	desc3.data = "data3";
	desc3.len = sizeof("data3");

	od_pstmt_desc_t desc4;
	desc4.data = "data4";
	desc4.len = sizeof("data4");

	od_pstmt_desc_t desc5;
	desc5.data = "data5";
	desc5.len = sizeof("data5");

	od_pstmt_t *unnamed1 = od_pstmt_create_or_get(global, desc1);
	test(unnamed1 != NULL);

	od_pstmt_t *unnamed2 = od_pstmt_create_or_get(global, desc2);
	test(unnamed2 != NULL);

	od_pstmt_t *p0 = od_pstmt_create_or_get(global, desc3);
	test(p0 != NULL);

	od_pstmt_t *p1 = od_pstmt_create_or_get(global, desc4);
	test(p1 != NULL);

	od_pstmt_t *dangling = od_pstmt_create_or_get(global, desc5);
	test(dangling != NULL);

	test(unnamed1 != unnamed2);
	test(unnamed1 != p0);
	test(unnamed1 != p1);
	test(unnamed1 != dangling);
	test(unnamed2 != p0);
	test(unnamed2 != p1);
	test(unnamed2 != dangling);
	test(p0 != p1);
	test(p0 != dangling);
	test(p1 != dangling);

	/* works correctly with unnamed */
	test(od_client_has_pstmt(client, "") == 0);
	test(od_client_get_pstmt(client, "") == NULL);

	test(od_client_add_pstmt(client, "", unnamed1) == 0);
	test(od_client_has_pstmt(client, "") == 1);
	test(od_client_get_pstmt(client, "") == unnamed1);

	od_pstmt_unref(unnamed1);

	/* allows to redefine unnamed statement */
	test(od_client_add_pstmt(client, "", unnamed2) == 0);
	test(od_client_has_pstmt(client, "") == 1);
	test(od_client_get_pstmt(client, "") == unnamed2);

	od_pstmt_unref(unnamed2);

	test(od_client_remove_pstmt(client, "") == 0);
	test(od_client_has_pstmt(client, "") == 0);
	test(od_client_get_pstmt(client, "") == NULL);

	/* some named prepared statements */

	test(od_client_has_pstmt(client, "P_0") == 0);
	test(od_client_get_pstmt(client, "P_0") == NULL);

	test(od_client_has_pstmt(client, "P_0") == 0);
	test(od_client_get_pstmt(client, "P_0") == NULL);

	test(od_client_add_pstmt(client, "P_0", p0) == 0);
	test(od_client_has_pstmt(client, "P_0") == 1);
	test(od_client_get_pstmt(client, "P_0") == p0);

	/* do not add twice */
	test(od_client_add_pstmt(client, "P_0", p0) == 1);
	test(od_client_has_pstmt(client, "P_0") == 1);
	test(od_client_get_pstmt(client, "P_0") == p0);

	test(od_client_remove_pstmt(client, "P_1") == 0);
	test(od_client_has_pstmt(client, "P_1") == 0);
	test(od_client_get_pstmt(client, "P_1") == NULL);

	test(od_client_add_pstmt(client, "P_1", p1) == 0);
	test(od_client_has_pstmt(client, "P_1") == 1);
	test(od_client_get_pstmt(client, "P_1") == p1);

	test(od_client_remove_pstmt(client, "P_0") == 0);

	test(od_client_has_pstmt(client, "P_1") == 1);
	test(od_client_get_pstmt(client, "P_1") == p1);
	test(od_client_has_pstmt(client, "P_0") == 0);
	test(od_client_get_pstmt(client, "P_0") == NULL);

	od_pstmt_unref(p0);
	od_pstmt_unref(p1);

	od_client_pstmts_clear(client);

	test(od_client_has_pstmt(client, "P_1") == 0);
	test(od_client_get_pstmt(client, "P_1") == NULL);

	test(od_client_add_pstmt(client, "dangling", dangling) == 0);
	test(od_client_has_pstmt(client, "dangling") == 1);
	test(od_client_get_pstmt(client, "dangling") == dangling);

	od_pstmt_unref(dangling);

	od_client_free(client);

	od_global_pstmts_map_free(global);
}

static void test_portal_client_hashmap(void)
{
	od_global_pstmt_map_t *global = od_global_pstmts_map_create(1);
	test(global != NULL);

	od_client_t *client = od_client_allocate();
	client->portals = od_client_portal_hashmap_create();
	test(client->portals != NULL);

	od_pstmt_desc_t desc1;
	desc1.data = "data";
	desc1.len = sizeof("data");

	od_pstmt_desc_t desc2;
	desc2.data = "data2";
	desc2.len = sizeof("data2");

	od_pstmt_desc_t desc3;
	desc3.data = "data3";
	desc3.len = sizeof("data3");

	od_pstmt_desc_t desc4;
	desc4.data = "data4";
	desc4.len = sizeof("data4");

	od_pstmt_desc_t desc5;
	desc5.data = "data5";
	desc5.len = sizeof("data5");

	od_pstmt_t *unnamed1 = od_pstmt_create_or_get(global, desc1);
	test(unnamed1 != NULL);

	od_pstmt_t *unnamed2 = od_pstmt_create_or_get(global, desc2);
	test(unnamed2 != NULL);

	od_pstmt_t *p0 = od_pstmt_create_or_get(global, desc3);
	test(p0 != NULL);

	od_pstmt_t *p1 = od_pstmt_create_or_get(global, desc4);
	test(p1 != NULL);

	od_pstmt_t *dangling = od_pstmt_create_or_get(global, desc5);
	test(dangling != NULL);

	test(unnamed1 != unnamed2);
	test(unnamed1 != p0);
	test(unnamed1 != p1);
	test(unnamed1 != dangling);
	test(unnamed2 != p0);
	test(unnamed2 != p1);
	test(unnamed2 != dangling);
	test(p0 != p1);
	test(p0 != dangling);
	test(p1 != dangling);

	/* unnamed portal works */
	test(od_client_get_portal(client, "") == NULL);
	test(od_client_add_portal(client, "", unnamed1) == 0);
	test(od_client_get_portal(client, "") == unnamed1);
	od_pstmt_unref(unnamed1);

	/* portals are always upserted (we are trying our best of tracking) */
	test(od_client_add_portal(client, "", unnamed2) == 0);
	test(od_client_get_portal(client, "") == unnamed2);
	od_pstmt_unref(unnamed2);

	test(od_client_remove_portal(client, "") == 0);
	test(od_client_get_portal(client, "") == NULL);

	/* named portals */
	test(od_client_get_portal(client, "portal_0") == NULL);

	test(od_client_add_portal(client, "portal_0", p0) == 0);
	test(od_client_get_portal(client, "portal_0") == p0);

	/* re-binding the same portal replaces the pstmt */
	test(od_client_add_portal(client, "portal_0", p1) == 0);
	test(od_client_get_portal(client, "portal_0") == p1);

	test(od_client_add_portal(client, "portal_1", p0) == 0);
	test(od_client_get_portal(client, "portal_1") == p0);

	/* remove non-existent is a no-op */
	test(od_client_remove_portal(client, "nope") == 0);
	test(od_client_get_portal(client, "portal_0") == p1);
	test(od_client_get_portal(client, "portal_1") == p0);

	test(od_client_remove_portal(client, "portal_0") == 0);
	test(od_client_get_portal(client, "portal_0") == NULL);
	test(od_client_get_portal(client, "portal_1") == p0);

	od_client_portals_clear(client);

	od_pstmt_unref(p0);
	od_pstmt_unref(p1);

	test(od_client_get_portal(client, "portal_1") == NULL);

	/* dangling entry survives until free */
	test(od_client_add_portal(client, "dangling", dangling) == 0);
	test(od_client_get_portal(client, "dangling") == dangling);

	od_pstmt_unref(dangling);

	od_client_free(client);

	od_global_pstmts_map_free(global);
}

static void test_pstmt_server_hashmap(void)
{
	od_global_pstmt_map_t *global = od_global_pstmts_map_create(1);
	test(global != NULL);

	od_server_t *server = od_server_allocate(1);

	od_pstmt_desc_t desc3;
	desc3.data = "data3";
	desc3.len = sizeof("data3");

	od_pstmt_desc_t desc4;
	desc4.data = "data4";
	desc4.len = sizeof("data4");

	od_pstmt_desc_t desc5;
	desc5.data = "data5";
	desc5.len = sizeof("data5");

	od_pstmt_t *p0 = od_pstmt_create_or_get(global, desc3);
	test(p0 != NULL);

	od_pstmt_t *p1 = od_pstmt_create_or_get(global, desc4);
	test(p1 != NULL);

	od_pstmt_t *dangling = od_pstmt_create_or_get(global, desc5);
	test(dangling != NULL);

	test(p0 != p1);
	test(p0 != dangling);
	test(p1 != dangling);

	test(od_server_has_pstmt(server, p0) == 0);
	test(od_server_has_pstmt(server, p1) == 0);

	test(od_server_remove_pstmt(server, p0) == 0);
	test(od_server_remove_pstmt(server, p1) == 0);

	test(od_server_add_pstmt(server, p0) == 0);
	test(od_server_has_pstmt(server, p0) == 1);
	test(od_server_has_pstmt(server, p1) == 0);

	test(od_server_add_pstmt(server, p1) == 0);
	test(od_server_has_pstmt(server, p0) == 1);
	test(od_server_has_pstmt(server, p1) == 1);

	test(od_server_remove_pstmt(server, p1) == 0);
	test(od_server_has_pstmt(server, p0) == 1);
	test(od_server_has_pstmt(server, p1) == 0);

	od_server_pstmts_clear(server);
	test(od_server_has_pstmt(server, p0) == 0);
	test(od_server_has_pstmt(server, p1) == 0);

	od_pstmt_unref(p0);
	od_pstmt_unref(p1);

	test(od_server_add_pstmt(server, dangling) == 0);
	test(od_server_has_pstmt(server, dangling) == 1);

	od_pstmt_unref(dangling);

	od_server_free(server);

	od_global_pstmts_map_free(global);
}

static void test_impl(void *a)
{
	(void)a;

	test_pstmt_global();
	test_refs();

	test_pstmt_client_hashmap();
	test_portal_client_hashmap();
	test_pstmt_server_hashmap();
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
