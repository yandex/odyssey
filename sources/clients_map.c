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

/* clients by pid map */

static inline mm_hash_t clients_pid_map_hash(const void *a)
{
	uint32_t pid = *(const uint32_t *)a;
	return mm_xxh64_hash(&pid, sizeof(pid), 0);
}

static inline int clients_pid_map_cmp(const void *a, const void *b)
{
	uint32_t x = *(const uint32_t *)a;
	uint32_t y = *(const uint32_t *)b;
	return !(x == y);
}

od_global_clients_pid_map_t *od_global_clients_pid_map_create(size_t nbuckets,
							       size_t nlocks)
{
	mm_hashmap_t *hm =
		mm_hashmap_create(nbuckets, nlocks, sizeof(uint32_t),
				  sizeof(od_client_t *), clients_pid_map_cmp,
				  clients_pid_map_hash, NULL, NULL, NULL);
	if (hm == NULL) {
		return NULL;
	}

	od_global_clients_pid_map_t *map =
		od_malloc(sizeof(od_global_clients_pid_map_t));
	if (map == NULL) {
		mm_hashmap_free(hm);
		return NULL;
	}

	map->hm = hm;
	mm_mutex_init(&map->lock);
	return map;
}

void od_global_clients_pid_map_free(od_global_clients_pid_map_t *map)
{
	if (map == NULL) {
		return;
	}

	mm_hashmap_free(map->hm);
	od_free(map);
}

int od_global_clients_pid_map_add(od_global_clients_pid_map_t *map,
				  od_client_t *client)
{
	mm_hashmap_keylock_t klock;
	uint32_t pid = client->key.key_pid;
	mm_mutex_lock2(&map->lock);
	int rc = mm_hashmap_lock_key(map->hm, &klock, &pid, MM_HASHMAP_CREATE);
	if (rc == -1) {
		mm_mutex_unlock(&map->lock);
		return -1;
	}
	void *val = mm_hashmap_kvp_val(map->hm, klock.kvp);
	memcpy(val, &client, sizeof(od_client_t *));
	mm_hashmap_unlock_key(map->hm, &klock);
	mm_mutex_unlock(&map->lock);
	return 0;
}

int od_global_clients_pid_map_remove(od_global_clients_pid_map_t *map,
				     od_client_t *client)
{
	mm_hashmap_keylock_t klock;
	uint32_t pid = client->key.key_pid;
	mm_mutex_lock2(&map->lock);
	int rc = mm_hashmap_lock_key(map->hm, &klock, &pid, 0);
	if (rc == -1 || !klock.found) {
		/* the key lock is not held when not found */
		mm_mutex_unlock(&map->lock);
		return rc == -1 ? -1 : 0;
	}
	mm_hashmap_remove(map->hm, &klock);
	mm_mutex_unlock(&map->lock);
	return 0;
}

od_client_t *od_global_clients_pid_map_get(od_global_clients_pid_map_t *map,
					   uint32_t pid)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(map->hm, &klock, &pid, 0);
	if (rc == -1 || !klock.found) {
		/* the key lock is not held when not found */
		return NULL;
	}
	od_client_t *client =
		*(od_client_t **)mm_hashmap_kvp_val(map->hm, klock.kvp);
	mm_hashmap_unlock_key(map->hm, &klock);
	return client;
}

void od_global_clients_pid_map_lock(od_global_clients_pid_map_t *map)
{
	mm_mutex_lock2(&map->lock);
}

void od_global_clients_pid_map_unlock(od_global_clients_pid_map_t *map)
{
	mm_mutex_unlock(&map->lock);
}
