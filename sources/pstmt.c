/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <kiwi/kiwi.h>
#include <machinarium/machinarium.h>

#include <od_memory.h>
#include <global.h>
#include <client.h>
#include <instance.h>
#include <util.h>
#include <client.h>
#include <server.h>
#include <pstmt.h>
#include <query.h>

/* XXX: randomize seed ? */
static const uint64_t hash_seed = 0;

/*
 * client hash map
 * "P_0" -> *od_prepared_stmt_t
 */

static mm_hash_t xxh_str_ptr(const void *data)
{
	const char *ptr = *(const char **)data;

	return mm_xxh64_hash(ptr, strlen(ptr), hash_seed);
}

static int str_ptr_cmp(const void *k1, const void *k2)
{
	const char *s1 = *(const char **)k1;
	const char *s2 = *(const char **)k2;

	return strcmp(s1, s2);
}

static void str_ptr_dtor(void *k)
{
	od_free(*(char **)k);
}

static int str_ptr_copy(void *dst, const void *src)
{
	const char *key = *(const char **)src;

	char *copy = od_strdup(key);
	if (copy == NULL) {
		return -1;
	}

	memcpy(dst, &copy, sizeof(char *));

	return 0;
}

od_client_stmt_map_t *od_client_pstmt_map_create(void)
{
	od_client_stmt_map_t *map = od_malloc(sizeof(*map));
	if (map == NULL) {
		return NULL;
	}

	mm_hashmap_ctl_t ctl = {
		.nbuckets = 50 /* XXX: big enough? */,
		.keysz = sizeof(char *),
		.valsz = sizeof(od_pstmt_t *),
		.kcmp = str_ptr_cmp,
		.khash = xxh_str_ptr,
		.kdtor = str_ptr_dtor,
		.kcopy = str_ptr_copy,
		/* pooled shells well above the live set of a serial
		 * client, bounded so the pool is not the retention
		 * multiplier across the whole client population */
		.nfree_max = 64,
	};

	/* map is client-private: no locks */
	if (mm_hashmap_init(&map->hm, &ctl,
			    MM_HM_KEYDTOR | MM_HM_KEYCOPY | MM_HM_NFREE_MAX) ==
	    -1) {
		od_free(map);
		return NULL;
	}

	map->unnamed = NULL;

	return map;
}

od_client_stmt_map_t *od_client_portal_map_create(void)
{
	od_client_stmt_map_t *map = od_malloc(sizeof(*map));
	if (map == NULL) {
		return NULL;
	}

	mm_hashmap_ctl_t ctl = {
		.nbuckets = 50 /* XXX: big enough? */,
		.keysz = sizeof(char *),
		.valsz = sizeof(od_pstmt_t *),
		.kcmp = str_ptr_cmp,
		.khash = xxh_str_ptr,
		.kdtor = str_ptr_dtor,
		.kcopy = str_ptr_copy,
		/* portals live per transaction and the client is serial,
		 * so a couple dozen shells cover the hot reuse without
		 * inflating per-client retention */
		.nfree_max = 32,
	};

	/* map is client-private: no locks */
	if (mm_hashmap_init(&map->hm, &ctl,
			    MM_HM_KEYDTOR | MM_HM_KEYCOPY | MM_HM_NFREE_MAX) ==
	    -1) {
		od_free(map);
		return NULL;
	}

	map->unnamed = NULL;

	return map;
}

static int unref_pstmt_entry(mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp,
			     void **argv)
{
	(void)argv;

	od_pstmt_t *pstmt = *(od_pstmt_t **)mm_hashmap_kvp_val(hm, kvp);

	od_pstmt_unref(pstmt);

	return 0;
}

/*
 * unnamed ("") fast-path: the slot holds the same reference the map
 * holds for named entries, so the refcount invariants are identical.
 * the "" key never enters the hashmap.
 */

static void client_stmt_map_set_unnamed(od_client_stmt_map_t *map,
					od_pstmt_t *pstmt)
{
	/* ref first: a re-add of the same stmt stays alive across the
	 * swap, mirroring the locked hashmap path */
	od_pstmt_t *old = map->unnamed;

	od_pstmt_ref(pstmt);
	map->unnamed = pstmt;

	if (old != NULL) {
		od_pstmt_unref(old);
	}
}

static void client_stmt_map_remove_unnamed(od_client_stmt_map_t *map)
{
	od_pstmt_t *old = map->unnamed;

	if (old != NULL) {
		map->unnamed = NULL;
		od_pstmt_unref(old);
	}
}

static void client_stmt_map_free(od_client_stmt_map_t *map)
{
	client_stmt_map_remove_unnamed(map);

	mm_hashmap_foreach(&map->hm, unref_pstmt_entry, NULL);

	mm_hashmap_destroy(&map->hm);
	od_free(map);
}

void od_client_pstmt_map_free(od_client_stmt_map_t *map)
{
	if (map == NULL) {
		return;
	}

	client_stmt_map_free(map);
}

void od_client_portal_map_free(od_client_stmt_map_t *map)
{
	if (map == NULL) {
		return;
	}

	client_stmt_map_free(map);
}

od_pstmt_t *od_client_get_pstmt(od_client_t *client, const char *name)
{
	return od_client_get_pstmt_hash(
		client, name,
		od_hm_name_hash(name, (uint32_t)strlen(name) + 1));
}

od_pstmt_t *od_client_get_pstmt_hash(od_client_t *client, const char *name,
				     mm_hash_t hash)
{
	if (name[0] == '\0') {
		return client->prep_stmt_ids->unnamed;
	}

	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(&client->prep_stmt_ids->hm,
					       &klock, &name, hash,
					       0 /* do not create */);
	(void)rc;

	if (klock.found) {
		od_pstmt_t *ps = *(od_pstmt_t **)mm_hashmap_kvp_val(
			&client->prep_stmt_ids->hm, klock.kvp);
		/* no real concurrent access - can unlock now and return */
		mm_hashmap_unlock_key(&client->prep_stmt_ids->hm, &klock);
		return ps;
	}

	/* lock is not held if key not found */
	return NULL;
}

int od_client_add_pstmt(od_client_t *client, const char *name,
			od_pstmt_t *pstmt)
{
	return od_client_add_pstmt_hash(
		client, name, od_hm_name_hash(name, (uint32_t)strlen(name) + 1),
		pstmt);
}

int od_client_add_pstmt_hash(od_client_t *client, const char *name,
			     mm_hash_t hash, od_pstmt_t *pstmt)
{
	if (name[0] == '\0') {
		/*
		 * its ok to redefine "" statement
		 * (unnamed statemnt is rewritten every Parse,
		 * not generate ErrorResponse about
		 * statement already exists)
		 */
		client_stmt_map_set_unnamed(client->prep_stmt_ids, pstmt);
		return 0;
	}

	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(
		&client->prep_stmt_ids->hm, &klock, &name, hash,
		MM_HASHMAP_CREATE /* do create */);
	if (rc == -1) {
		/* cant create and lock is not held */
		return rc;
	}

	int ret = 0;

	if (!klock.found) {
		/* new - save */
		void *val = mm_hashmap_kvp_val(&client->prep_stmt_ids->hm,
					       klock.kvp);
		memcpy(val, &pstmt, sizeof(const od_pstmt_t *));
		od_pstmt_ref(pstmt);
		ret = 0;
	} else {
		/* already exists - skip */
		ret = 1;
	}

	/* no real concurrent access - can unlock now and return */
	mm_hashmap_unlock_key(&client->prep_stmt_ids->hm, &klock);

	return ret;
}

int od_client_has_pstmt(od_client_t *client, const char *name)
{
	return od_client_get_pstmt(client, name) != NULL;
}

int od_client_remove_pstmt(od_client_t *client, const char *name)
{
	return od_client_remove_pstmt_hash(
		client, name,
		od_hm_name_hash(name, (uint32_t)strlen(name) + 1));
}

int od_client_remove_pstmt_hash(od_client_t *client, const char *name,
				mm_hash_t hash)
{
	if (name[0] == '\0') {
		client_stmt_map_remove_unnamed(client->prep_stmt_ids);
		return 0;
	}

	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(&client->prep_stmt_ids->hm,
					       &klock, &name, hash,
					       0 /* do not create */);
	(void)rc;

	if (klock.kvp != NULL) {
		od_pstmt_t *pstmt = *(od_pstmt_t **)mm_hashmap_kvp_val(
			&client->prep_stmt_ids->hm, klock.kvp);
		mm_hashmap_remove(&client->prep_stmt_ids->hm, &klock);
		if (pstmt != NULL) {
			od_pstmt_unref(pstmt);
		}
	}

	/* if not found - lock is not held */

	return 0;
}

void od_client_pstmts_clear(od_client_t *client)
{
	client_stmt_map_remove_unnamed(client->prep_stmt_ids);

	mm_hashmap_foreach(&client->prep_stmt_ids->hm, unref_pstmt_entry, NULL);

	mm_hashmap_clear(&client->prep_stmt_ids->hm);
}

/*
 * client portal hash map
 * "portal_name" -> od_pstmt_t*
 *
 * portals are created by Bind (destination portal name) and destroyed by
 * Close Portal, transaction end, or DISCARD ALL / DEALLOCATE ALL.
 *
 * the value is a non-owning pointer into the global pstmt map. the pointer
 * is stable because the map holds its own reference while the entry exists
 * (entries are removed only when the last external reference is dropped,
 * see od_pstmt_unref).
 */

od_pstmt_t *od_client_get_portal(od_client_t *client, const char *portal_name)
{
	return od_client_get_portal_hash(
		client, portal_name,
		od_hm_name_hash(portal_name,
				(uint32_t)strlen(portal_name) + 1));
}

od_pstmt_t *od_client_get_portal_hash(od_client_t *client,
				      const char *portal_name, mm_hash_t hash)
{
	if (portal_name[0] == '\0') {
		return client->portals->unnamed;
	}

	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(&client->portals->hm, &klock,
					       &portal_name, hash,
					       0 /* do not create */);
	(void)rc;

	if (klock.found) {
		od_pstmt_t *ps = *(od_pstmt_t **)mm_hashmap_kvp_val(
			&client->portals->hm, klock.kvp);
		mm_hashmap_unlock_key(&client->portals->hm, &klock);
		return ps;
	}

	return NULL;
}

int od_client_add_portal(od_client_t *client, const char *portal_name,
			 od_pstmt_t *pstmt)
{
	return od_client_add_portal_hash(
		client, portal_name,
		od_hm_name_hash(portal_name, (uint32_t)strlen(portal_name) + 1),
		pstmt);
}

int od_client_add_portal_hash(od_client_t *client, const char *portal_name,
			      mm_hash_t hash, od_pstmt_t *pstmt)
{
	/*
	 * note: unnamed portal "" is silently overwritten on every Bind.
	 * for named portals PG requires an explicit Close before re-Bind, but
	 * the server will enforce that - we just store the mapping here
	 */
	if (portal_name[0] == '\0') {
		client_stmt_map_set_unnamed(client->portals, pstmt);
		return 0;
	}

	mm_hashmap_keylock_t klock;
	od_pstmt_t *to_unref = NULL;

	int rc = mm_hashmap_lock_key_with_hash(
		&client->portals->hm, &klock, &portal_name, hash,
		MM_HASHMAP_CREATE /* do create */);
	if (rc == -1) {
		return rc;
	}

	void *val = mm_hashmap_kvp_val(&client->portals->hm, klock.kvp);
	to_unref = *(od_pstmt_t **)(val);
	memcpy(val, &pstmt, sizeof(const od_pstmt_t *));
	od_pstmt_ref(pstmt);
	mm_hashmap_unlock_key(&client->portals->hm, &klock);

	if (to_unref != NULL) {
		od_pstmt_unref(to_unref);
	}

	return 0;
}

int od_client_remove_portal(od_client_t *client, const char *portal_name)
{
	return od_client_remove_portal_hash(
		client, portal_name,
		od_hm_name_hash(portal_name,
				(uint32_t)strlen(portal_name) + 1));
}

int od_client_remove_portal_hash(od_client_t *client, const char *portal_name,
				 mm_hash_t hash)
{
	if (portal_name[0] == '\0') {
		client_stmt_map_remove_unnamed(client->portals);
		return 0;
	}

	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(&client->portals->hm, &klock,
					       &portal_name, hash,
					       0 /* do not create */);
	(void)rc;

	if (klock.kvp != NULL) {
		od_pstmt_t *pstmt = *(od_pstmt_t **)mm_hashmap_kvp_val(
			&client->portals->hm, klock.kvp);

		mm_hashmap_remove(&client->portals->hm, &klock);

		if (pstmt != NULL) {
			od_pstmt_unref(pstmt);
		}
	}

	return 0;
}

void od_client_portals_clear(od_client_t *client)
{
	client_stmt_map_remove_unnamed(client->portals);

	mm_hashmap_foreach(&client->portals->hm, unref_pstmt_entry, NULL);

	mm_hashmap_clear(&client->portals->hm);
}

/*
 * server hashmap
 * "odyssey_pstmt_0" -> od_server_pstmt_slot_t
 *
 * the slot embeds the SIEVE queue node and the visited bit
 */

typedef struct {
	/* borrowed: the hashmap entry owns the ref */
	od_pstmt_t *pstmt;
	od_list_t link;
	int visited;
} od_server_pstmt_slot_t;

static int unref_server_pstmt_slot(mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp,
				   void **argv)
{
	(void)argv;

	od_server_pstmt_slot_t *slot = mm_hashmap_kvp_val(hm, kvp);

	od_pstmt_unref(slot->pstmt);

	return 0;
}

static void server_pstmt_queue_reset(od_server_pstmt_map_t *map)
{
	od_list_init(&map->fifo);
	map->hand = &map->fifo;
	map->count = 0;
}

static mm_hash_t xxh_str_inplace(const void *data)
{
	const char *key = data;

	return mm_xxh64_hash(data, strlen(key), hash_seed);
}

static int str_inplace_cmp(const void *k1, const void *k2)
{
	const char *s1 = k1;
	const char *s2 = k2;

	return strcmp(s1, s2);
}

od_server_pstmt_map_t *od_server_pstmt_map_create(void)
{
	od_server_pstmt_map_t *map = od_malloc(sizeof(*map));
	if (map == NULL) {
		return NULL;
	}

	mm_hashmap_ctl_t ctl = {
		.nbuckets = 1000 /* XXX: big enough? */,
		.keysz = sizeof(od_pstmt_name_t),
		.valsz = sizeof(od_server_pstmt_slot_t),
		.kcmp = str_inplace_cmp,
		.khash = xxh_str_inplace,
		/* console foreach may run concurrently with writers */
		.nlocks = 1,
	};

	if (mm_hashmap_init(&map->hm, &ctl, MM_HM_NLOCKS) == -1) {
		od_free(map);
		return NULL;
	}

	od_list_init(&map->fifo);
	map->hand = &map->fifo;
	map->count = 0;

	return map;
}

size_t od_server_pstmt_count(od_server_t *server)
{
	if (server->prep_stmts == NULL) {
		return 0;
	}

	return server->prep_stmts->count;
}

void od_server_pstmts_free(od_server_t *server)
{
	/* the hashmap has no value dtor, unref explicitly */
	mm_hashmap_foreach(&server->prep_stmts->hm, unref_server_pstmt_slot,
			   NULL);

	mm_hashmap_destroy(&server->prep_stmts->hm);
	od_free(server->prep_stmts);
	server->prep_stmts = NULL;
}

int od_server_has_pstmt(od_server_t *server, const od_pstmt_t *pstmt)
{
	od_server_pstmt_map_t *map = server->prep_stmts;
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(&map->hm, &klock, pstmt->name,
					       pstmt->name_hash,
					       0 /* do not create */);
	(void)rc;

	if (klock.found) {
		/* SIEVE hit */
		od_server_pstmt_slot_t *slot =
			mm_hashmap_kvp_val(&map->hm, klock.kvp);
		slot->visited = 1;

		/* no real concurrent access - can unlock now and return */
		mm_hashmap_unlock_key(&map->hm, &klock);
		return 1;
	}

	/* not exists - no lock held */
	return 0;
}

int od_server_add_pstmt(od_server_t *server, od_pstmt_t *pstmt)
{
	od_server_pstmt_map_t *map = server->prep_stmts;
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(
		&map->hm, &klock, pstmt->name, pstmt->name_hash,
		MM_HASHMAP_CREATE /* do create */);
	if (rc == -1) {
		/* cant create and lock is not held */
		return rc;
	}

	/*
	 * note: server's pstmt has generated name like odyssey_pstmt_1337
	 * so there is no unnamed statements at all and no possibility for unref
	 * because no pstmts can be upserted
	 */

	int ret = 0;

	if (!klock.found) {
		od_server_pstmt_slot_t *slot =
			mm_hashmap_kvp_val(&map->hm, klock.kvp);
		slot->pstmt = pstmt;
		slot->visited = 0;
		od_list_init(&slot->link);
		od_list_append(&map->fifo, &slot->link);
		map->count++;

		od_pstmt_ref(pstmt);
		ret = 0;
	} else {
		/* count a duplicate add as a hit */
		od_server_pstmt_slot_t *slot =
			mm_hashmap_kvp_val(&map->hm, klock.kvp);
		slot->visited = 1;
		ret = 1;
	}

	/* no real concurrent access - can unlock now and return */
	mm_hashmap_unlock_key(&map->hm, &klock);

	return ret;
}

int od_server_remove_pstmt(od_server_t *server, const od_pstmt_t *pstmt)
{
	od_server_pstmt_map_t *map = server->prep_stmts;
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key_with_hash(&map->hm, &klock, pstmt->name,
					       pstmt->name_hash,
					       0 /* do not create */);
	(void)rc;

	if (klock.found) {
		od_server_pstmt_slot_t *slot =
			mm_hashmap_kvp_val(&map->hm, klock.kvp);

		/* the hand must not point at the node being unlinked */
		if (map->hand == &slot->link) {
			map->hand = slot->link.next;
		}

		od_list_unlink(&slot->link);
		map->count--;

		od_pstmt_t *p = slot->pstmt;
		mm_hashmap_remove(&map->hm, &klock);
		if (p != NULL) {
			od_pstmt_unref(p);
		}
	}

	/* if not found - lock is not held */

	return 0;
}

void od_server_pstmts_clear(od_server_t *server)
{
	od_server_pstmt_map_t *map = server->prep_stmts;

	mm_hashmap_foreach(&map->hm, unref_server_pstmt_slot, NULL);

	mm_hashmap_clear(&map->hm);

	server_pstmt_queue_reset(map);
}

const od_pstmt_t *od_server_pstmt_kvp_pstmt(mm_hashmap_t *hm,
					    mm_hashmap_kvp_t *kvp)
{
	const od_server_pstmt_slot_t *slot = mm_hashmap_kvp_val_const(hm, kvp);

	return slot->pstmt;
}

int od_server_pstmt_evict_overflow(od_server_t *server, size_t cap,
				   machine_msg_t *stream)
{
	if (server->prep_stmts == NULL) {
		return 0;
	}

	od_server_pstmt_map_t *map = server->prep_stmts;

	int evicted = 0;

	/* three sweeps always reach a fixed point, see the SIEVE paper */
	size_t iterations = 0;
	size_t bound = 3 * map->count + 3;

	while (map->count > cap && iterations++ < bound) {
		if (map->hand == &map->fifo) {
			map->hand = map->fifo.next;
			if (map->hand == &map->fifo) {
				break;
			}
		}

		od_server_pstmt_slot_t *slot = od_container_of(
			map->hand, od_server_pstmt_slot_t, link);
		od_pstmt_t *pstmt = slot->pstmt;

		/* advance first: the node may be freed below */
		map->hand = slot->link.next;

		if (slot->visited) {
			slot->visited = 0;
			continue;
		}

		/* refs == 2: only the global map and this server hold it */
		uint64_t refs = atomic_load_explicit(&pstmt->refs,
						     memory_order_acquire);
		if (refs > 2) {
			continue;
		}

		mm_hashmap_keylock_t klock;
		int rc = mm_hashmap_lock_key_with_hash(&map->hm, &klock,
						       pstmt->name,
						       pstmt->name_hash,
						       0 /* do not create */);
		if (rc == -1) {
			continue;
		}

		if (!klock.found) {
			/* out of sync with the map - drop the node and its reference */
			mm_hashmap_unlock_key(&map->hm, &klock);
			od_list_unlink(&slot->link);
			map->count--;
			od_pstmt_unref(pstmt);
			continue;
		}

		od_release_assert((od_server_pstmt_slot_t *)mm_hashmap_kvp_val(
					  &map->hm, klock.kvp) == slot);
		od_release_assert(((od_server_pstmt_slot_t *)mm_hashmap_kvp_val(
					   &map->hm, klock.kvp))
					  ->pstmt == pstmt);

		machine_msg_t *next = kiwi_fe_write_close(
			stream, 'S', pstmt->name, strlen(pstmt->name) + 1);
		if (next == NULL) {
			mm_hashmap_unlock_key(&map->hm, &klock);
			return -1;
		}
		stream = next;

		od_list_unlink(&slot->link);
		map->count--;

		mm_hashmap_remove(&map->hm, &klock);

		/* last use of pstmt: the unref may free it */
		od_pstmt_unref(pstmt);

		evicted++;
	}

	return evicted;
}

/*
 * global map
 *
 * key:   od_pstmt_desc_t (inline, non-owning — desc.data points into
 *        the value's desc bytes, so the key is a "view" of the value)
 * value: od_pstmt_t*
 */

static mm_hash_t xxh_pstmt_desc(const void *data)
{
	const od_pstmt_desc_t *desc = data;

	return mm_xxh64_hash(desc->data, desc->len, hash_seed);
}

static int pstmt_desc_cmp(const void *k1, const void *k2)
{
	const od_pstmt_desc_t *d1 = k1;
	const od_pstmt_desc_t *d2 = k2;

	if (d1->len != d2->len) {
		return 1;
	}

	return memcmp(d1->data, d2->data, d1->len);
}

static inline __attribute__((unused)) int
pstmt_is_correct_deletion(od_pstmt_t *p)
{
	/*
	 * need to check ref correctly because pstmt has a race on creation
	 * (can be created and then deleted immediately because some other
	 * thread created same pstmt)
	 *
	 * so either it is the last ref (ref == 1)
	 * or it is not initialized pstmt (ref == 0 and empty description)
	 */
	uint64_t r = atomic_load_explicit(&p->refs, memory_order_acquire);
	return r == 1 || (r == 0 && p->desc.data == NULL);
}

void od_pstmt_unpin(od_pstmt_t *pstmt)
{
	/*
	 * the decrement must be acq_rel: the release part publishes
	 * this thread's accesses, while the acquire part (which only
	 * matters when the last pin is observed) synchronizes with the
	 * release unpins of the other pin holders, so all their
	 * accesses to the pstmt happen before the destruction below;
	 * the map lock does not cover every final-unpin path, so it
	 * cannot be relied upon here
	 *
	 * note: an acquire fence after a release-only decrement would
	 * be enough per the memory model, but tsan does not model
	 * fences, so the acquire has to be part of the RMW itself
	 */
	uint64_t v = atomic_fetch_sub_explicit(&pstmt->pins, 1,
					       memory_order_acq_rel);
	if (v > 1) {
		return;
	}

	od_assert(pstmt_is_correct_deletion(pstmt));
	od_query_ctx_reset(&pstmt->query_ctx);
	od_free(pstmt);
}

static void pstmt_desc_val_dtor(void *val)
{
	od_pstmt_t *pstmt = *(od_pstmt_t **)val;

	if (pstmt == NULL) {
		/*
		 * the entry was created but its struct was never set -
		 * this happens when the struct allocation failed and the
		 * entry was removed right away
		 */
		return;
	}

	od_pstmt_unpin(pstmt);
}

static void pstmt_init_new(od_global_pstmt_map_t *hm, od_pstmt_t *out)
{
	uint64_t num = atomic_fetch_add_explicit(&hm->counter, 1,
						 memory_order_relaxed);
	od_release_assert(num <= OD_MAX_PSTMT_NUM);

	atomic_init(&out->pins, 1);
	atomic_init(&out->refs, 1);
	out->source = hm;

	od_snprintf(out->name, sizeof(od_pstmt_name_t), "%s%" PRIu64,
		    OD_PSTMT_NAME_PREFIX, num);

	/*
	 * cache the name hash so the server map lookups never re-hash
	 * the name; must stay equal to the map khash (xxh_str_inplace,
	 * strlen-based), otherwise lookups via mm_hashmap_lock_key
	 * would land in the wrong bucket
	 */
	out->name_hash = mm_xxh64_hash(out->name, strlen(out->name), hash_seed);
}

od_global_pstmt_map_t *od_global_pstmts_map_create(size_t nlocks)
{
	mm_hashmap_ctl_t ctl = {
		.nbuckets = 10000 /* XXX: big enough? */,
		.keysz = sizeof(od_pstmt_desc_t),
		.valsz = sizeof(od_pstmt_t *),
		.kcmp = pstmt_desc_cmp,
		.khash = xxh_pstmt_desc,
		/* key does not own desc.data; value owns the pstmt pointer */
		.vdtor = pstmt_desc_val_dtor,
		/* no key copy — raw memcpy, data ptr overwritten after
		 * insert */
		.nlocks = nlocks,
	};

	mm_hashmap_t *hm =
		mm_hashmap_create(&ctl, MM_HM_NLOCKS | MM_HM_VALDTOR);

	if (hm == NULL) {
		return NULL;
	}

	od_global_pstmt_map_t *gm = od_malloc(sizeof(od_global_pstmt_map_t));
	if (gm == NULL) {
		mm_hashmap_free(hm);
		return NULL;
	}

	gm->hm = hm;
	atomic_init(&gm->counter, 0);

	return gm;
}

void od_global_pstmts_map_free(od_global_pstmt_map_t *hm)
{
	mm_hashmap_free(hm->hm);
	od_free(hm);
}

od_pstmt_t *od_pstmt_create_or_get(od_global_pstmt_map_t *pstmts,
				   const od_pstmt_desc_t desc,
				   od_linear_alloc_t *arena,
				   const od_config_query_parsing_t *parsing)
{
	mm_hashmap_keylock_t klock;
	int rc;
	od_pstmt_desc_t *key;
	od_pstmt_t *value;
	od_pstmt_t **slot;

	/*
	 * lock_key will creates a key that is binary-copy of desc
	 *
	 * need to remember: it does not owns desc.data
	 */
	mm_hash_t dhash = xxh_pstmt_desc(&desc);
	rc = mm_hashmap_lock_key_with_hash(pstmts->hm, &klock, &desc, dhash,
					   MM_HASHMAP_CREATE);
	if (rc == -1) {
		return NULL;
	}

	slot = (od_pstmt_t **)mm_hashmap_kvp_val(pstmts->hm, klock.kvp);
	key = (od_pstmt_desc_t *)mm_hashmap_kvp_key(pstmts->hm, klock.kvp);

	if (!klock.found) {
		/*
		 * init new prep stmt: the struct and the desc bytes are
		 * one allocation, desc.data points to the trailing bytes
		 */
		value = od_malloc(sizeof(od_pstmt_t) + desc.len);
		if (value == NULL) {
			/* slot is still NULL, the value dtor handles this */
			mm_hashmap_remove(pstmts->hm, &klock);
			return NULL;
		}

		/*
		 * the dtor owns the struct from now on, even if the
		 * steps below fail
		 */
		*slot = value;

		memset(value, 0, sizeof(od_pstmt_t));
		pstmt_init_new(pstmts, value);
		value->desc_hash = dhash;

		if (desc.data == NULL) {
			mm_hashmap_remove(pstmts->hm, &klock);
			return NULL;
		}

		value->desc.len = desc.len;
		value->desc.data = value->desc_data;
		memcpy(value->desc_data, desc.data, desc.len);

		/*
		 * rewrite the key's data pointer to point into the value
		 * so the key becomes a non-owning view of the value, instead of
		 * memcpy of find key (desc)
		 */
		key->data = value->desc_data;

		/*
		 * parse the query text once and cache the query context flags
		 * (is_discard_all, is_unlisten_all, is_deallocate_all,
		 *  deallocate_name) so that plan_execute / process_discard /
		 * process_unlisten can avoid re-parsing on every Execute.
		 */
		od_query_parse_fill_ctx(value->desc.data,
					strlen(value->desc.data), arena,
					&value->query_ctx, parsing);
		value->query_parser_mode = parsing->mode;
	} else {
		/* the key already exists and has a copy of desc.data, do nothing */
		value = *slot;
	}

	/* the call-side now holds the ref too */
	od_pstmt_ref(value);

	mm_hashmap_unlock_key(pstmts->hm, &klock);

	return value;
}

void od_global_pstmt_try_remove(od_global_pstmt_map_t *gm, od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	int rc;
	uint64_t refs;

	rc = mm_hashmap_lock_key_with_hash(gm->hm, &klock, &pstmt->desc,
					   pstmt->desc_hash, 0 /* no create */);
	if (rc == -1) {
		return;
	}

	if (klock.kvp == NULL) {
		/*
		 * while this thread was going from the refs decrement to
		 * the lock acquisition, another thread removed the entry;
		 * the no-create lock_key reports a miss with the bucket
		 * lock already released, so there is nothing to unlock
		 */
		return;
	}

	/*
	 * the key could be re-created for the same desc (DEALLOCATE +
	 * Parse) and now belongs to another pstmt instance - our own
	 * entry is not in the map anymore, do not touch this one
	 */
	if (*(od_pstmt_t **)mm_hashmap_kvp_val(gm->hm, klock.kvp) != pstmt) {
		mm_hashmap_unlock_key(gm->hm, &klock);
		return;
	}

	/*
	 * note: ref can be done only with the lock held (create_or_get)
	 * or from the ref, that is already valid, so no race here
	 */
	refs = atomic_load_explicit(&pstmt->refs, memory_order_acquire);
	if (refs > 1) {
		mm_hashmap_unlock_key(gm->hm, &klock);
	} else {
		mm_hashmap_remove(gm->hm, &klock);
	}
}

int od_global_pstmts_has_pstmt(od_global_pstmt_map_t *gm,
			       const od_pstmt_desc_t desc)
{
	mm_hashmap_keylock_t klock;
	int rc;

	rc = mm_hashmap_lock_key_with_hash(gm->hm, &klock, &desc,
					   xxh_pstmt_desc(&desc),
					   0 /* do not create */);
	if (rc == -1) {
		return 0;
	}

	if (klock.found) {
		/* yes, not so thread-safe check, but this function is only used in tests */
		mm_hashmap_unlock_key(gm->hm, &klock);
		return 1;
	}

	/* no lock held */

	return 0;
}

static int foreach_wrapper(mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp, void **argv)
{
	union {
		od_global_pstmt_cb cb;
		void *p;
	} p;
	void *arg;

	p.p = argv[0];
	arg = argv[1];

	const od_pstmt_t *pstmt =
		*(od_pstmt_t *const *)mm_hashmap_kvp_val(hm, kvp);

	return p.cb(pstmt, arg);
}

void od_global_pstmt_foreach(od_global_pstmt_map_t *gm, od_global_pstmt_cb cb,
			     void *arg)
{
	union {
		od_global_pstmt_cb ccb;
		void *argv[2];
	} a;
	a.ccb = cb;
	a.argv[1] = arg;

	mm_hashmap_foreach(gm->hm, foreach_wrapper, a.argv);
}

/* helpers */

char *od_pstmt_name_from_parse_len(machine_msg_t *msg, uint32_t *name_len)
{
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);
	kiwi_prepared_statement_t desc;
	if (kiwi_be_read_parse_dest(data, size, &desc)) {
		return NULL;
	}

	*name_len = desc.operator_name_len;

	return desc.operator_name;
}

od_pstmt_desc_t od_pstmt_desc_from_parse(machine_msg_t *msg)
{
	od_pstmt_desc_t ret;
	memset(&ret, 0, sizeof(od_pstmt_desc_t));

	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);
	kiwi_prepared_statement_t desc;
	if (kiwi_be_read_parse_dest(data, size, &desc) == 0) {
		ret.data = desc.description;
		ret.len = desc.description_len;
	}

	return ret;
}

machine_msg_t *od_pstmt_parse_of(const od_pstmt_t *pstmt)
{
	const char *srv_name = pstmt->name;
	const od_pstmt_desc_t *desc = &pstmt->desc;

	machine_msg_t *pmsg = kiwi_fe_write_parse_description(
		NULL, srv_name, strlen(srv_name) + 1 /* include zero byte */,
		desc->data, desc->len);

	return pmsg;
}

machine_msg_t *od_pstmt_describe_of(const od_pstmt_t *pstmt)
{
	const char *srv_name = pstmt->name;

	machine_msg_t *dmsg = kiwi_fe_write_describe(NULL, 'S', srv_name,
						     strlen(srv_name) + 1);

	return dmsg;
}
