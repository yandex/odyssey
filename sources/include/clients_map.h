#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*
 * global clients map: kiwi_key_t -> od_client_t *
 *
 * used to find a client by its backend key, e.g. to route
 * cancel requests or to synchronize close/detach under
 * the per-key lock
 */

#include <machinarium/ds/hm.h>

#include <kiwi/key.h>

#include <types.h>

typedef struct od_global_clients_map od_global_clients_map_t;

struct od_global_clients_map {
	mm_hashmap_t *hm;
};

od_global_clients_map_t *od_global_clients_map_create(size_t nlocks);
void od_global_clients_map_free(od_global_clients_map_t *map);

/*
 * hash of the key as used by the map (xxh64 with key_pid as the
 * seed); stable for the lifetime of the key
 */
mm_hash_t od_global_clients_map_hash_key(const void *key);

int od_global_clients_map_lock(od_global_clients_map_t *map,
			       const kiwi_key_t *key,
			       mm_hashmap_keylock_t *klock);

/* as _lock, but with the hash supplied by the caller */
int od_global_clients_map_lock_hash(od_global_clients_map_t *map,
				    const kiwi_key_t *key, mm_hash_t hash,
				    mm_hashmap_keylock_t *klock);

/*
 * take the per-key shard mutex without the bucket chain walk:
 * klock.found is always 0.  for callers that only need to
 * serialize against lock/lock_hash under the same key (router
 * detach/close); release with od_global_clients_map_unlock
 */
int od_global_clients_map_lock_shard(od_global_clients_map_t *map,
				     mm_hashmap_keylock_t *klock,
				     mm_hash_t hash);

void od_global_clients_map_unlock(od_global_clients_map_t *map,
				  mm_hashmap_keylock_t *klock);

/*
 * register the client under its key; fails if the key is
 * already taken
 */
int od_global_clients_map_add(od_global_clients_map_t *map,
			      od_client_t *client);
int od_global_clients_map_remove(od_global_clients_map_t *map,
				 od_client_t *client);

kiwi_key_t od_global_clients_map_get_key(od_global_clients_map_t *map,
					 mm_hashmap_keylock_t *klock);
od_client_t *od_global_clients_map_get_val(od_global_clients_map_t *map,
					   mm_hashmap_keylock_t *klock);
