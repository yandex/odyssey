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
mm_hash_t od_global_clients_map_hash_key(const void *keyptr)
{
	const kiwi_key_t *key = keyptr;
	return mm_xxh64_hash(key->key, key->key_len, key->key_pid);
}

static inline int od_global_clients_map_hm_cmp(const void *a, const void *b)
{
	const kiwi_key_t *x = a, *y = b;
	return !kiwi_key_cmp(x, y);
}

od_global_clients_map_t *od_global_clients_map_create(size_t nlocks)
{
	mm_hashmap_ctl_t ctl = {
		.nbuckets = 32768,
		.keysz = sizeof(kiwi_key_t),
		.valsz = sizeof(od_client_t *),
		.kcmp = od_global_clients_map_hm_cmp,
		.khash = od_global_clients_map_hash_key,
		.nlocks = nlocks,
	};

	mm_hashmap_t *hm = mm_hashmap_create(&ctl, MM_HM_NLOCKS);
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

int od_global_clients_map_lock_hash(od_global_clients_map_t *map,
				    const kiwi_key_t *key, mm_hash_t hash,
				    mm_hashmap_keylock_t *klock)
{
	return mm_hashmap_lock_key_with_hash(map->hm, klock, key, hash, 0);
}

int od_global_clients_map_lock_shard(od_global_clients_map_t *map,
				     mm_hashmap_keylock_t *klock,
				     mm_hash_t hash)
{
	return mm_hashmap_lock_shard(map->hm, klock, hash);
}

int od_global_clients_map_lock(od_global_clients_map_t *map,
			       const kiwi_key_t *key,
			       mm_hashmap_keylock_t *klock)
{
	return od_global_clients_map_lock_hash(
		map, key, od_global_clients_map_hash_key(key), klock);
}

void od_global_clients_map_unlock(od_global_clients_map_t *map,
				  mm_hashmap_keylock_t *klock)
{
	mm_hashmap_unlock_key(map->hm, klock);
}

int od_global_clients_map_add(od_global_clients_map_t *map, od_client_t *client)
{
	/*
	 * the key is immutable from here on: cache its hash so
	 * detach/close/remove do not re-hash the key bytes
	 */
	client->key_hash = od_global_clients_map_hash_key(&client->key);

	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(map->hm, &klock, &client->key,
					       client->key_hash,
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
	int rc = od_global_clients_map_lock_hash(map, &client->key,
						 client->key_hash, &klock);
	if (rc == -1) {
		return rc;
	}
	od_release_assert(klock.found);
	mm_hashmap_remove(map->hm, &klock);
	return 0;
}

kiwi_key_t od_global_clients_map_get_key(od_global_clients_map_t *map,
					 mm_hashmap_keylock_t *klock)
{
	od_assert(klock->found);
	return *(kiwi_key_t *)mm_hashmap_kvp_key(map->hm, klock->kvp);
}

od_client_t *od_global_clients_map_get_val(od_global_clients_map_t *map,
					   mm_hashmap_keylock_t *klock)
{
	od_assert(klock->found);
	return *(od_client_t **)mm_hashmap_kvp_val(map->hm, klock->kvp);
}
