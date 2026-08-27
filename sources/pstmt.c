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

mm_hashmap_t *od_client_pstmt_hashmap_create(void)
{
	return mm_hashmap_create(
		50 /* XXX: big enough? */,
		1 /* nlocks = 1, no fully-concurrent access to client hashmap */,
		sizeof(char *) /* key size */,
		sizeof(od_pstmt_t *) /* val size */, str_ptr_cmp /* key cmp */,
		xxh_str_ptr /* key hash */, str_ptr_dtor /* key dtor */,
		NULL /* no need to free the pointer from global table */,
		str_ptr_copy /* key copy */);
}

static int unref_pstmt_entry(mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp,
			     void **argv)
{
	(void)argv;

	od_pstmt_t *pstmt = *(od_pstmt_t **)mm_hashmap_kvp_val(hm, kvp);

	od_pstmt_unref(pstmt);

	return 0;
}

void od_client_pstmt_hashmap_free(mm_hashmap_t *hm)
{
	mm_hashmap_foreach(hm, unref_pstmt_entry, NULL);

	mm_hashmap_free(hm);
}

od_pstmt_t *od_client_get_pstmt(od_client_t *client, const char *name)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(client->prep_stmt_ids, &klock, &name,
				     0 /* do not create */);
	(void)rc;

	if (klock.found) {
		od_pstmt_t *ps = *(od_pstmt_t **)mm_hashmap_kvp_val(
			client->prep_stmt_ids, klock.kvp);
		/* no real concurrent access - can unlock now and return */
		mm_hashmap_unlock_key(client->prep_stmt_ids, &klock);
		return ps;
	}

	/* lock is not held if key not found */
	return NULL;
}

int od_client_add_pstmt(od_client_t *client, const char *name,
			od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	od_pstmt_t *to_unref = NULL;
	const int unnamed = name[0] == '\0';

	int rc = mm_hashmap_lock_key(client->prep_stmt_ids, &klock, &name,
				     MM_HASHMAP_CREATE /* do create */);
	if (rc == -1) {
		/* cant create and lock is not held */
		return rc;
	}

	int ret = 0;

	void *val = mm_hashmap_kvp_val(client->prep_stmt_ids, klock.kvp);
	if (unnamed && (*(void **)val) != NULL) {
		/*
		 * its ok to redefine "" statement
		 * (unnamed statemnt is rewritten every Parse,
		 * not generate ErrorResponse about
		 * statement already exists)
		 *
		 * so need to not forget to unref previous pstmt
		 */
		to_unref = *(od_pstmt_t **)val;
	}

	if (!klock.found || unnamed) {
		/* new or "" - save */
		memcpy(val, &pstmt, sizeof(const od_pstmt_t *));
		od_pstmt_ref(pstmt);
		ret = 0;
	} else {
		/* already exists - skip */
		ret = 1;
	}

	/* no real concurrent access - can unlock now and return */
	mm_hashmap_unlock_key(client->prep_stmt_ids, &klock);

	if (to_unref != NULL) {
		od_pstmt_unref(to_unref);
	}

	return ret;
}

int od_client_has_pstmt(od_client_t *client, const char *name)
{
	return od_client_get_pstmt(client, name) != NULL;
}

int od_client_remove_pstmt(od_client_t *client, const char *name)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(client->prep_stmt_ids, &klock, &name,
				     0 /* do not create */);
	(void)rc;

	if (klock.kvp != NULL) {
		od_pstmt_t *pstmt = *(od_pstmt_t **)mm_hashmap_kvp_val(
			client->prep_stmt_ids, klock.kvp);
		mm_hashmap_remove(client->prep_stmt_ids, &klock);
		if (pstmt != NULL) {
			od_pstmt_unref(pstmt);
		}
	}

	/* if not found - lock is not held */

	return 0;
}

void od_client_pstmts_clear(od_client_t *client)
{
	mm_hashmap_foreach(client->prep_stmt_ids, unref_pstmt_entry, NULL);

	mm_hashmap_clear(client->prep_stmt_ids);
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

mm_hashmap_t *od_client_portal_hashmap_create(void)
{
	return mm_hashmap_create(
		50 /* XXX: big enough? */,
		1 /* nlocks = 1, no fully-concurrent access */,
		sizeof(char *) /* key size */,
		sizeof(od_pstmt_t *) /* value size */,
		str_ptr_cmp /* key cmp */, xxh_str_ptr /* key hash */,
		str_ptr_dtor /* key dtor */,
		NULL /* no need to free the pointer from global table */,
		str_ptr_copy /* key copy */);
}

void od_client_portal_hashmap_free(mm_hashmap_t *hm)
{
	mm_hashmap_foreach(hm, unref_pstmt_entry, NULL);

	mm_hashmap_free(hm);
}

od_pstmt_t *od_client_get_portal(od_client_t *client, const char *portal_name)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(client->portals, &klock, &portal_name,
				     0 /* do not create */);
	(void)rc;

	if (klock.found) {
		od_pstmt_t *ps = *(od_pstmt_t **)mm_hashmap_kvp_val(
			client->portals, klock.kvp);
		mm_hashmap_unlock_key(client->portals, &klock);
		return ps;
	}

	return NULL;
}

int od_client_add_portal(od_client_t *client, const char *portal_name,
			 od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	od_pstmt_t *to_unref = NULL;

	int rc = mm_hashmap_lock_key(client->portals, &klock, &portal_name,
				     MM_HASHMAP_CREATE /* do create */);
	if (rc == -1) {
		return rc;
	}

	/*
	 * note: unnamed portal "" is silently overwritten on every Bind.
	 * for named portals PG requires an explicit Close before re-Bind, but
	 * the server will enforce that - we just store the mapping here
	 */

	void *val = mm_hashmap_kvp_val(client->portals, klock.kvp);
	to_unref = *(od_pstmt_t **)(val);
	memcpy(val, &pstmt, sizeof(const od_pstmt_t *));
	od_pstmt_ref(pstmt);
	mm_hashmap_unlock_key(client->portals, &klock);

	if (to_unref != NULL) {
		od_pstmt_unref(to_unref);
	}

	return 0;
}

int od_client_remove_portal(od_client_t *client, const char *portal_name)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(client->portals, &klock, &portal_name,
				     0 /* do not create */);
	(void)rc;

	if (klock.kvp != NULL) {
		od_pstmt_t *pstmt = *(od_pstmt_t **)mm_hashmap_kvp_val(
			client->portals, klock.kvp);

		mm_hashmap_remove(client->portals, &klock);

		if (pstmt != NULL) {
			od_pstmt_unref(pstmt);
		}
	}

	return 0;
}

void od_client_portals_clear(od_client_t *client)
{
	mm_hashmap_foreach(client->portals, unref_pstmt_entry, NULL);

	mm_hashmap_clear(client->portals);
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

static void server_pstmt_queue_reset(od_server_t *server)
{
	od_list_init(&server->pstmt_fifo);
	server->pstmt_hand = &server->pstmt_fifo;
	server->pstmt_count = 0;
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

mm_hashmap_t *od_server_pstmt_hashmap_create(void)
{
	return mm_hashmap_create(
		100 /* XXX: big enough? */,
		1 /* nlocks = 1, no fully-concurrent access to server hashmap */,
		sizeof(od_pstmt_name_t) /* key size */,
		sizeof(od_server_pstmt_slot_t) /* value size */,
		str_inplace_cmp /* key cmp */, xxh_str_inplace /* key hash */,
		NULL /* no need to free on inplace str */,
		NULL /* no need to free the pointer from global table */,
		NULL /* no key copy function */
	);
}

void od_server_pstmts_free(od_server_t *server)
{
	/* the hashmap has no value dtor, unref explicitly */
	mm_hashmap_foreach(server->prep_stmts, unref_server_pstmt_slot, NULL);

	mm_hashmap_free(server->prep_stmts);
	server->prep_stmts = NULL;

	server_pstmt_queue_reset(server);
}

int od_server_has_pstmt(od_server_t *server, const od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(server->prep_stmts, &klock, pstmt->name,
				     0 /* do not create */);
	(void)rc;

	if (klock.found) {
		/* SIEVE hit */
		od_server_pstmt_slot_t *slot =
			mm_hashmap_kvp_val(server->prep_stmts, klock.kvp);
		slot->visited = 1;

		/* no real concurrent access - can unlock now and return */
		mm_hashmap_unlock_key(server->prep_stmts, &klock);
		return 1;
	}

	/* not exists - no lock held */
	return 0;
}

int od_server_add_pstmt(od_server_t *server, od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(server->prep_stmts, &klock, pstmt->name,
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
			mm_hashmap_kvp_val(server->prep_stmts, klock.kvp);
		slot->pstmt = pstmt;
		slot->visited = 0;
		od_list_init(&slot->link);
		od_list_append(&server->pstmt_fifo, &slot->link);
		server->pstmt_count++;

		od_pstmt_ref(pstmt);
		ret = 0;
	} else {
		/* count a duplicate add as a hit */
		od_server_pstmt_slot_t *slot =
			mm_hashmap_kvp_val(server->prep_stmts, klock.kvp);
		slot->visited = 1;
		ret = 1;
	}

	/* no real concurrent access - can unlock now and return */
	mm_hashmap_unlock_key(server->prep_stmts, &klock);

	return ret;
}

int od_server_remove_pstmt(od_server_t *server, const od_pstmt_t *pstmt)
{
	mm_hashmap_keylock_t klock;
	int rc = mm_hashmap_lock_key(server->prep_stmts, &klock, pstmt->name,
				     0 /* do not create */);
	(void)rc;

	if (klock.found) {
		od_server_pstmt_slot_t *slot =
			mm_hashmap_kvp_val(server->prep_stmts, klock.kvp);

		/* the hand must not point at the node being unlinked */
		if (server->pstmt_hand == &slot->link) {
			server->pstmt_hand = slot->link.next;
		}

		od_list_unlink(&slot->link);
		server->pstmt_count--;

		od_pstmt_t *p = slot->pstmt;
		mm_hashmap_remove(server->prep_stmts, &klock);
		if (p != NULL) {
			od_pstmt_unref(p);
		}
	}

	/* if not found - lock is not held */

	return 0;
}

void od_server_pstmts_clear(od_server_t *server)
{
	mm_hashmap_foreach(server->prep_stmts, unref_server_pstmt_slot, NULL);

	mm_hashmap_clear(server->prep_stmts);

	server_pstmt_queue_reset(server);
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

	int evicted = 0;

	/* three sweeps always reach a fixed point, see the SIEVE paper */
	size_t iterations = 0;
	size_t bound = 3 * server->pstmt_count + 3;

	while (server->pstmt_count > cap && iterations++ < bound) {
		if (server->pstmt_hand == &server->pstmt_fifo) {
			server->pstmt_hand = server->pstmt_fifo.next;
			if (server->pstmt_hand == &server->pstmt_fifo) {
				break;
			}
		}

		od_server_pstmt_slot_t *slot = od_container_of(
			server->pstmt_hand, od_server_pstmt_slot_t, link);
		od_pstmt_t *pstmt = slot->pstmt;

		/* advance first: the node may be freed below */
		server->pstmt_hand = slot->link.next;

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
		int rc =
			mm_hashmap_lock_key(server->prep_stmts, &klock,
					    pstmt->name, 0 /* do not create */);
		if (rc == -1) {
			continue;
		}

		if (!klock.found) {
			/* out of sync with the map - drop the node and its reference */
			mm_hashmap_unlock_key(server->prep_stmts, &klock);
			od_list_unlink(&slot->link);
			server->pstmt_count--;
			od_pstmt_unref(pstmt);
			continue;
		}

		od_release_assert((od_server_pstmt_slot_t *)mm_hashmap_kvp_val(
					  server->prep_stmts, klock.kvp) ==
				  slot);
		od_release_assert(((od_server_pstmt_slot_t *)mm_hashmap_kvp_val(
					   server->prep_stmts, klock.kvp))
					  ->pstmt == pstmt);

		machine_msg_t *next = kiwi_fe_write_close(
			stream, 'S', pstmt->name, strlen(pstmt->name) + 1);
		if (next == NULL) {
			mm_hashmap_unlock_key(server->prep_stmts, &klock);
			return -1;
		}
		stream = next;

		od_list_unlink(&slot->link);
		server->pstmt_count--;

		mm_hashmap_remove(server->prep_stmts, &klock);

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
 *        the value's desc.data, so the key is a "view" of the value)
 * value: od_pstmt_t (inline, owns its own copy of desc.data)
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

static void pstmt_desc_val_dtor(void *val)
{
	od_pstmt_t *pstmt = val;
	od_assert(atomic_load_explicit(&pstmt->refs, memory_order_acquire) ==
		  1);
	od_free(pstmt->desc.data);
}

static void pstmt_init_new(od_global_pstmt_map_t *hm, od_pstmt_t *out)
{
	uint64_t num = atomic_fetch_add_explicit(&hm->counter, 1,
						 memory_order_relaxed);
	od_release_assert(num <= OD_MAX_PSTMT_NUM);

	atomic_init(&out->refs, 1);
	out->source = hm;

	od_snprintf(out->name, sizeof(od_pstmt_name_t), "%s%" PRIu64,
		    OD_PSTMT_NAME_PREFIX, num);
}

od_global_pstmt_map_t *od_global_pstmts_map_create(size_t nlocks)
{
	mm_hashmap_t *hm = mm_hashmap_create(
		10000 /* XXX: big enough? */, nlocks,
		sizeof(od_pstmt_desc_t) /* key size */,
		sizeof(od_pstmt_t) /* value size */,
		pstmt_desc_cmp /* key comparator */,
		xxh_pstmt_desc /* key hash */,
		NULL /* key does not own desc.data */,
		pstmt_desc_val_dtor /* value owns desc.data */,
		NULL /* no key copy — raw memcpy, data ptr overwritten after insert */
	);

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
				   const od_pstmt_desc_t desc)
{
	mm_hashmap_keylock_t klock;
	int rc;
	od_pstmt_desc_t *key;
	od_pstmt_t *value;

	/*
	 * lock_key will creates a key that is binary-copy of desc
	 *
	 * need to remember: it does not owns desc.data
	 */
	rc = mm_hashmap_lock_key(pstmts->hm, &klock, &desc,
				 MM_HASHMAP_CREATE /* create if not exists */);
	if (rc == -1) {
		return NULL;
	}

	value = (od_pstmt_t *)mm_hashmap_kvp_val(pstmts->hm, klock.kvp);
	key = (od_pstmt_desc_t *)mm_hashmap_kvp_key(pstmts->hm, klock.kvp);

	if (!klock.found) {
		/* init new prep stmt */
		memset(value, 0, sizeof(od_pstmt_t));
		pstmt_init_new(pstmts, value);

		value->desc = od_pstmt_desc_copy(desc);
		if (value->desc.data == NULL) {
			mm_hashmap_remove(pstmts->hm, &klock);
			return NULL;
		}

		/*
		 * rewrite the key's data pointer to point into the value
		 * so the key becomes a non-owning view of the value, instead of
		 * memcpy of find key (desc)
		 */
		key->data = value->desc.data;
	} else {
		/* the key already exists and has a copy of desc.data, do nothing */
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

	rc = mm_hashmap_lock_key(gm->hm, &klock, &pstmt->desc,
				 0 /* no create */);
	if (rc == -1) {
		return;
	}

	/*
	 * every pstmt must be created from global hashmap,
	 * so no need to check the klock.kvp != NULL
	 */
	od_assert(klock.kvp != NULL);
	od_assert((od_pstmt_t *)mm_hashmap_kvp_val(gm->hm, klock.kvp) == pstmt);

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

	rc = mm_hashmap_lock_key(gm->hm, &klock, &desc, 0 /* do not create */);
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
		(const od_pstmt_t *)mm_hashmap_kvp_val(hm, kvp);

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

char *od_pstmt_name_from_parse(machine_msg_t *msg)
{
	char *data = machine_msg_data(msg);
	int size = machine_msg_size(msg);
	kiwi_prepared_statement_t desc;
	if (kiwi_be_read_parse_dest(data, size, &desc)) {
		return NULL;
	}

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

od_pstmt_desc_t od_pstmt_desc_copy(const od_pstmt_desc_t desc)
{
	if (desc.data == NULL) {
		return desc;
	}

	od_pstmt_desc_t copy;
	memset(&copy, 0, sizeof(od_pstmt_desc_t));

	copy.data = od_malloc(desc.len);
	if (copy.data != NULL) {
		copy.len = desc.len;
		memcpy(copy.data, desc.data, desc.len);
		return copy;
	}

	return copy;
}
