/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machinarium.h>

#include <od_memory.h>
#include <clients_map.h>
#include <client.h>

static inline mm_hash_t od_global_clients_map_hm_hash(const void *a)
{
	const kiwi_key_t *key = a;
	uint32_t buf[2] = { key->key_pid, key->key };
	return mm_xxh64_hash(buf, sizeof(buf), 0);
}

static inline int od_global_clients_map_hm_cmp(const void *a, const void *b)
{
	const kiwi_key_t *x = a, *y = b;
	return !(x->key_pid == y->key_pid && x->key == y->key);
}

od_global_clients_map_t *od_global_clients_map_create(size_t nlocks)
{
	mm_hashmap_t *hm = mm_hashmap_create(32768, nlocks, sizeof(kiwi_key_t),
					     sizeof(od_client_t *),
					     od_global_clients_map_hm_cmp,
					     od_global_clients_map_hm_hash,
					     NULL, NULL, NULL);
	if (hm == NULL) {
		return NULL;
	}

	od_global_clients_map_t *map =
		od_malloc(sizeof(od_global_clients_map_t));
	if (map == NULL) {
		mm_hashmap_free(hm);
		return NULL;
	}

	map->hm = hm;
	return map;
}

void od_global_clients_map_free(od_global_clients_map_t *map)
{
	if (map == NULL) {
		return;
	}

	mm_hashmap_free(map->hm);
	od_free(map);
}

int od_global_clients_map_lock(od_global_clients_map_t *map,
			       const kiwi_key_t *key,
			       mm_hashmap_keylock_t *klock)
{
	return mm_hashmap_lock_key(map->hm, klock, key, 0);
}

void od_global_clients_map_unlock(od_global_clients_map_t *map,
				  mm_hashmap_keylock_t *klock)
{
	mm_hashmap_unlock_key(map->hm, klock);
}

int od_global_clients_map_add(od_global_clients_map_t *map, od_client_t *client)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(map->hm, &klock, &client->key,
				     MM_HASHMAP_CREATE);
	if (rc == -1) {
		return -1;
	}
	if (klock.found) {
		od_global_clients_map_unlock(map, &klock);
		return -1;
	}
	void *val = mm_hashmap_kvp_val(map->hm, klock.kvp);
	memcpy(val, &client, sizeof(od_client_t *));
	od_global_clients_map_unlock(map, &klock);
	return 0;
}

int od_global_clients_map_remove(od_global_clients_map_t *map,
				 od_client_t *client)
{
	mm_hashmap_keylock_t klock;
	int rc = od_global_clients_map_lock(map, &client->key, &klock);
	if (rc == -1) {
		return rc;
	}
	od_release_assert(klock.found);
	mm_hashmap_remove(map->hm, &klock);
	return 0;
}
