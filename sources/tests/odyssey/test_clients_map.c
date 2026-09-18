
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <machinarium/machinarium.h>

#include <odyssey.h>
#include <client.h>
#include <clients_map.h>
#include <tests/odyssey_test.h>

#define CLIENTS_WORKERS 4
#define CLIENTS_PER_WORKER 32

static od_client_t *test_client_create(uint32_t key, uint32_t key_pid)
{
	od_client_t *client = od_client_allocate();
	test(client != NULL);
	client->key.key = key;
	client->key.key_pid = key_pid;
	return client;
}

static od_client_t *test_clients_map_get(od_global_clients_map_t *map,
					 const kiwi_key_t *key,
					 mm_hashmap_keylock_t *klock)
{
	int rc = od_global_clients_map_lock(map, key, klock);
	test(rc == 0);
	if (!klock->found) {
		return NULL;
	}
	return *(od_client_t **)mm_hashmap_kvp_val(map->hm, klock->kvp);
}

static int clients_map_count_cb(mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp,
				void **argv)
{
	(void)hm;
	(void)kvp;

	size_t *count = argv[0];
	(*count)++;

	return 0;
}

static size_t clients_map_count(od_global_clients_map_t *map)
{
	size_t count = 0;
	void *argv[1] = { &count };

	test(mm_hashmap_foreach(map->hm, clients_map_count_cb, argv) == 0);

	return count;
}

static void test_clients_map_create(void)
{
	od_global_clients_map_t *map = od_global_clients_map_create(1);
	test(map != NULL);
	test(map->hm != NULL);

	/* map is empty */
	test(clients_map_count(map) == 0);

	od_global_clients_map_free(map);

	/* NULL is tolerated */
	od_global_clients_map_free(NULL);
}

static void test_clients_add_lookup_remove(void)
{
	od_global_clients_map_t *map = od_global_clients_map_create(4);
	test(map != NULL);

	od_client_t *c1 = test_client_create(100, 1);
	od_client_t *c2 = test_client_create(100, 2);
	od_client_t *c3 = test_client_create(200, 3);

	mm_hashmap_keylock_t klock;

	/* empty map: key is not found */
	kiwi_key_t unknown;
	kiwi_key_init(&unknown);
	unknown.key = 777;
	unknown.key_pid = 777;
	test(test_clients_map_get(map, &unknown, &klock) == NULL);

	/* add */
	test(od_global_clients_map_add(map, c1) == 0);
	test(od_global_clients_map_add(map, c2) == 0);
	test(od_global_clients_map_add(map, c3) == 0);
	test(clients_map_count(map) == 3);

	/* same (key, key_pid) pair is not added twice */
	test(od_global_clients_map_add(map, c1) == -1);
	test(od_global_clients_map_add(map, c2) == -1);
	test(clients_map_count(map) == 3);

	/* duplicate add must not wedge the bucket: still operational */
	test(test_clients_map_get(map, &c3->key, &klock) == c3);
	od_global_clients_map_unlock(map, &klock);

	/* lookup by key returns the very same client */
	test(test_clients_map_get(map, &c1->key, &klock) == c1);
	od_global_clients_map_unlock(map, &klock);

	test(test_clients_map_get(map, &c2->key, &klock) == c2);
	od_global_clients_map_unlock(map, &klock);

	/* unknown key is still not found */
	test(test_clients_map_get(map, &unknown, &klock) == NULL);

	/* remove */
	test(od_global_clients_map_remove(map, c2) == 0);
	test(test_clients_map_get(map, &c2->key, &klock) == NULL);

	/* siblings are untouched */
	test(test_clients_map_get(map, &c1->key, &klock) == c1);
	od_global_clients_map_unlock(map, &klock);

	test(od_global_clients_map_remove(map, c1) == 0);
	test(od_global_clients_map_remove(map, c3) == 0);
	test(clients_map_count(map) == 0);

	od_client_free(c1);
	od_client_free(c2);
	od_client_free(c3);

	od_global_clients_map_free(map);
}

static void test_clients_get_key_val(void)
{
	od_global_clients_map_t *map = od_global_clients_map_create(4);
	test(map != NULL);

	od_client_t *c1 = test_client_create(100, 1);
	od_client_t *c2 = test_client_create(200, 2);

	test(od_global_clients_map_add(map, c1) == 0);
	test(od_global_clients_map_add(map, c2) == 0);

	mm_hashmap_keylock_t klock;

	/* get_key / get_val for a registered key */
	test(od_global_clients_map_lock(map, &c1->key, &klock) == 0);
	test(klock.found == 1);

	kiwi_key_t got_key = od_global_clients_map_get_key(map, &klock);
	test(got_key.key == c1->key.key);
	test(got_key.key_pid == c1->key.key_pid);

	od_client_t *got_val = od_global_clients_map_get_val(map, &klock);
	test(got_val == c1);

	od_global_clients_map_unlock(map, &klock);

	/* get_key / get_val for another registered key */
	test(od_global_clients_map_lock(map, &c2->key, &klock) == 0);
	test(klock.found == 1);
	test(od_global_clients_map_get_val(map, &klock) == c2);
	test(od_global_clients_map_get_key(map, &klock).key == c2->key.key);
	test(od_global_clients_map_get_key(map, &klock).key_pid ==
	     c2->key.key_pid);
	od_global_clients_map_unlock(map, &klock);

	/* unknown key: not found, get_key/get_val must not be used */
	kiwi_key_t unknown;
	kiwi_key_init(&unknown);
	unknown.key = 777;
	unknown.key_pid = 777;
	test(od_global_clients_map_lock(map, &unknown, &klock) == 0);
	test(klock.found == 0);
	od_global_clients_map_unlock(map, &klock);

	/* after remove the key is gone */
	test(od_global_clients_map_remove(map, c1) == 0);
	test(od_global_clients_map_lock(map, &c1->key, &klock) == 0);
	test(klock.found == 0);
	od_global_clients_map_unlock(map, &klock);

	od_client_free(c1);
	od_client_free(c2);

	od_global_clients_map_free(map);
}

typedef struct {
	od_global_clients_map_t *map;
	uint32_t key_base;
	int nclients;
} test_clients_worker_arg_t;

static void test_clients_worker(void *argp)
{
	test_clients_worker_arg_t *arg = argp;
	od_global_clients_map_t *map = arg->map;

	od_client_t *clients[CLIENTS_PER_WORKER];

	int i;
	for (i = 0; i < arg->nclients; i++) {
		clients[i] = test_client_create(arg->key_base + (uint32_t)i,
						arg->key_base);
		test(od_global_clients_map_add(map, clients[i]) == 0);
	}

	/* all own clients are visible */
	for (i = 0; i < arg->nclients; i++) {
		mm_hashmap_keylock_t klock;
		test(test_clients_map_get(map, &clients[i]->key, &klock) ==
		     clients[i]);
		od_global_clients_map_unlock(map, &klock);
	}

	for (i = 0; i < arg->nclients; i++) {
		test(od_global_clients_map_remove(map, clients[i]) == 0);
		od_client_free(clients[i]);
	}
}

static void test_clients_concurrent(void)
{
	od_global_clients_map_t *map = od_global_clients_map_create(
		64 /* locks, like the real map for a single worker */);
	test(map != NULL);

	int64_t machine_ids[CLIENTS_WORKERS];
	test_clients_worker_arg_t args[CLIENTS_WORKERS];

	for (int i = 0; i < CLIENTS_WORKERS; i++) {
		args[i].map = map;
		args[i].key_base = (uint32_t)((i + 1) * 100000);
		args[i].nclients = CLIENTS_PER_WORKER;

		char name[32];
		snprintf(name, sizeof(name), "clients_worker%d", i);
		machine_ids[i] =
			machine_create(name, test_clients_worker, &args[i]);
		test(machine_ids[i] > 0);
	}

	for (int i = 0; i < CLIENTS_WORKERS; i++) {
		test(machine_wait(machine_ids[i]) == 0);
	}

	/* every worker removed all of its clients */
	test(clients_map_count(map) == 0);

	od_global_clients_map_free(map);
}

static void test_impl(void *a)
{
	(void)a;

	test_clients_map_create();
	test_clients_add_lookup_remove();
	test_clients_get_key_val();
	test_clients_concurrent();
}

void odyssey_test_clients_map(void)
{
	machinarium_init();

	int64_t rc;
	rc = machine_create("test_impl", test_impl, NULL);
	test(rc > 0);

	test(machine_wait(rc) == 0);

	machinarium_free();
}
