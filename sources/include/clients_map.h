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
 * lock the bucket for the key; klock.found tells whether
 * a client with such key is registered
 * 
 * if klock.found = 0, then the unlock will occur automatically
 */
int od_global_clients_map_lock(od_global_clients_map_t *map,
			       const kiwi_key_t *key,
			       mm_hashmap_keylock_t *klock);
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
