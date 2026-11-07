#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <machinarium/machinarium.h>
#include <machinarium/mutex.h>
#include <machinarium/ds/hm.h>
#include <kiwi/key.h>
#include <tests/odyssey_test.h>

/* local mirrors of the production key shapes (pstmt.h is not
 * self-contained, its layout is pinned here instead):
 * od_pstmt_desc_t { void *data; size_t len; } and
 * od_pstmt_name_t char[OD_PSTMT_NAME_MAX_LEN == 32] with
 * "odyssey_pstmt_<1-17 digits>" names */
typedef struct {
	void *data;
	size_t len;
} test_desc_t;

typedef char test_pstmt_name_t[32];

/*
 * hash map distribution test.
 *
 * the map selects a bucket as `hash & bucket_mask` (power-of-2 sizes,
 * pg-style). this test pins down the guarantees for every production
 * scenario: each case mirrors a real call site (clients_map.c,
 * pstmt.c x4, sql_pstmt.c) with its exact key shape, hash function,
 * size hint and lock mode, and checks that
 *
 *  - sizes and lock shard counts round up to powers of two, masks
 *    are consistent with the actual sizes;
 *  - nothing collapses into one bucket: max chain length stays
 *    bounded, occupancy is in the poisson(0.75) ballpark;
 *  - insert / lookup / remove roundtrips are exact, including the
 *    key-copy and dtor paths of the pointer-key scenarios;
 *  - every bucket's mutex is the one its lock shard dictates.
 *
 * the hash inputs are deterministic (fixed seed), so the observed
 * chain lengths are stable across runs and platforms; the bounds are
 * set with x2+ headroom over the observed maxima.
 */

/* ---------- generic string helpers (inplace keys) ---------- */

static mm_hash_t inplace_str_hash(const void *data)
{
	const char *key = data;
	return mm_xxh64_hash(key, strlen(key), 0);
}

static int inplace_str_cmp(const void *k1, const void *k2)
{
	return strcmp(k1, k2);
}

/* ---------- ptr-string keys (== pstmt.c str_ptr_*) ---------- */

static mm_hash_t str_ptr_hash(const void *data)
{
	const char *ptr = *(const char **)data;
	return mm_xxh64_hash(ptr, strlen(ptr), 0);
}

static int str_ptr_cmp(const void *k1, const void *k2)
{
	return strcmp(*(const char **)k1, *(const char **)k2);
}

static int str_ptr_copy(void *dst, const void *src)
{
	const char *s = *(const char **)src;
	char *copy = strdup(s);
	if (copy == NULL) {
		return -1;
	}
	memcpy(dst, &copy, sizeof(char *));
	return 0;
}

static void str_ptr_dtor(void *key)
{
	free(*(char **)key);
}

/* ---------- od_pstmt_desc_t keys (== global pstmts map) ---------- */

static mm_hash_t desc_hash(const void *data)
{
	const test_desc_t *desc = data;
	return mm_xxh64_hash(desc->data, desc->len, 0);
}

static int desc_cmp(const void *k1, const void *k2)
{
	const test_desc_t *d1 = k1;
	const test_desc_t *d2 = k2;
	return d1->len != d2->len || memcmp(d1->data, d2->data, d1->len);
}

/* ---------- kiwi_key_t keys (== clients map) ---------- */

static mm_hash_t clients_key_hash(const void *data)
{
	const kiwi_key_t *key = data;
	return mm_xxh64_hash(key->key, key->key_len, key->key_pid);
}

static int clients_key_cmp(const void *a, const void *b)
{
	return !kiwi_key_cmp(a, b);
}

/* ---------- scenario plumbing ---------- */

#define SCENARIO_KEY_MAX 288

typedef void (*key_fill_fn)(void *keyobj, size_t i);

typedef struct {
	const char *name; /* scenario == real call site */
	mm_hashmap_ctl_t ctl;
	int flags;
	size_t nkeys;
	size_t nbuckets; /* expected roundup pow2 */
	size_t max_bound; /* max chain length stays below this */
	key_fill_fn fill; /* fills a SCENARIO_KEY_MAX key object */
} scenario_t;

static void fill_inplace_odyssey(size_t i, char *buf, size_t sz,
				 const char *fmt)
{
	/* "odyssey_pstmt_<n>": the generated name shape of
	 * od_pstmt_name gen (pstmt.c:678) */
	snprintf(buf, sz, fmt, i);
}

static void fill_str_ptr(void *keyobj, size_t i)
{
	/* key object is a char* slot; the map strdups it via kcopy */
	static char storage[SCENARIO_KEY_MAX];
	static char *slot = storage;

	fill_inplace_odyssey(i, storage, sizeof(storage), "odyssey_pstmt_%zu");
	memcpy(keyobj, &slot, sizeof(char *));
}

static void fill_str_ptr_portal(void *keyobj, size_t i)
{
	/* portal names are client-chosen (Bind message), more entropy
	 * in shapes than the generated "odyssey_pstmt_" family */
	static char storage[SCENARIO_KEY_MAX];
	static char *slot = storage;

	switch (i % 4) {
	case 0:
		snprintf(storage, sizeof(storage), "C_%zu", i);
		break;
	case 1:
		snprintf(storage, sizeof(storage), "portal_%zu", i);
		break;
	case 2:
		snprintf(storage, sizeof(storage), "%c%c_%zu",
			 (int)('a' + (i % 26)), (int)('A' + (i % 26)), i);
		break;
	default:
		snprintf(storage, sizeof(storage), "%zu", i);
		break;
	}
	memcpy(keyobj, &slot, sizeof(char *));
}

static void fill_client_pstmt(void *keyobj, size_t i)
{
	fill_str_ptr(keyobj, i);
}

static void fill_client_portal(void *keyobj, size_t i)
{
	fill_str_ptr_portal(keyobj, i);
}

static void fill_sql_name(void *keyobj, size_t i)
{
	/* statement names parsed from real traffic: mixed shapes */
	static char storage[SCENARIO_KEY_MAX];
	static char *slot = storage;

	switch (i % 3) {
	case 0:
		snprintf(storage, sizeof(storage), "stmt_%zu", i);
		break;
	case 1:
		snprintf(storage, sizeof(storage), "s%zu", i);
		break;
	default:
		snprintf(storage, sizeof(storage), "odyssey_pstmt_%zu", i);
		break;
	}
	memcpy(keyobj, &slot, sizeof(char *));
}

static void fill_server_pstmt(void *keyobj, size_t i)
{
	/* inplace od_pstmt_name_t, server-side: generated names (zero
	 * padded as od_pstmt_name_t always is) */
	memset(keyobj, 0, sizeof(test_pstmt_name_t));
	fill_inplace_odyssey(i, keyobj, sizeof(test_pstmt_name_t),
			     "odyssey_pstmt_%zu");
}

static void fill_global_desc(void *keyobj, size_t i)
{
	/* query-ish body of varying length (od_pstmt_desc_t shape) */
	static char storage[SCENARIO_KEY_MAX];
	test_desc_t desc;

	size_t body_suffix = i % 128;
	int n = snprintf(
		storage, sizeof(storage),
		"SELECT a%d, b FROM t%zu WHERE id = $1 AND x = %zu /* %zu */",
		(int)(i % 8), (size_t)(i % 16), body_suffix, i);
	test(n > 0 && (size_t)n < sizeof(storage));

	desc.data = storage;
	desc.len = (size_t)n;
	memcpy(keyobj, &desc, sizeof(desc));
}

static void fill_clients_key(void *keyobj, size_t i)
{
	/* kiwi_key_t: pid as seed, mostly 4-byte secrets (the common
	 * backend-key-data case), mixed with longer opaque keys */
	kiwi_key_t key;
	kiwi_key_init(&key);
	key.key_pid = (uint32_t)i;

	if (i % 5 == 0) {
		key.key_len = 32;
		for (size_t b = 0; b < key.key_len; b++) {
			key.key[b] = (char)(i * 31 + b * 7);
		}
	} else {
		key.key_len = KIWI_CANCEL_KEY_MIN_LEN;
		uint32_t secret = (uint32_t)(i * 2654435761u);
		key.key[0] = (char)(secret >> 24);
		key.key[1] = (char)(secret >> 16);
		key.key[2] = (char)(secret >> 8);
		key.key[3] = (char)secret;
	}

	memcpy(keyobj, &key, sizeof(key));
}

static void run_scenario(const scenario_t *s)
{
	mm_hashmap_t *hm = mm_hashmap_create(&s->ctl, s->flags);
	test(hm != NULL);

	test(hm->nbuckets == s->nbuckets);
	test((hm->nbuckets & (hm->nbuckets - 1)) == 0);
	test(hm->bucket_mask == hm->nbuckets - 1);

	if (s->flags & MM_HM_NLOCKS) {
		test((hm->nlocks & (hm->nlocks - 1)) == 0);
		test(hm->lock_mask == hm->nlocks - 1);

		for (size_t b = 0; b < hm->nbuckets; b++) {
			mm_mutex_t *expected = mm_mutex_padded_at(
				(mm_mutex_padded_t *)hm->locks,
				b & hm->lock_mask);
			test(hm->buckets[b].mu == expected);
		}
	}

	/* insert */
	for (size_t i = 0; i < s->nkeys; i++) {
		char keyobj[SCENARIO_KEY_MAX];

		memset(keyobj, 0, sizeof(keyobj));
		s->fill(keyobj, i);

		mm_hashmap_keylock_t kl;
		int rc =
			mm_hashmap_lock_key(hm, &kl, keyobj, MM_HASHMAP_CREATE);
		test(rc == 0);
		test(kl.found == 0); /* no false insert positives */
		test(kl.kvp != NULL);

		*(int64_t *)mm_hashmap_kvp_val(hm, kl.kvp) = (int64_t)i;
		mm_hashmap_unlock_key(hm, &kl);
	}

	/* exact lookups (also after keyobj re-fill: key data must not
	 * be aliased into the map for copy scenarios) */
	for (size_t i = 0; i < s->nkeys; i++) {
		char keyobj[SCENARIO_KEY_MAX];

		memset(keyobj, 0, sizeof(keyobj));
		s->fill(keyobj, i);

		mm_hashmap_keylock_t kl;
		int rc = mm_hashmap_lock_key(hm, &kl, keyobj, 0);
		test(rc == 0);
		test(kl.found == 1);
		test(kl.kvp != NULL);
		test(*(int64_t *)mm_hashmap_kvp_val(hm, kl.kvp) == (int64_t)i);
		mm_hashmap_unlock_key(hm, &kl);
	}

	/* remove every 3rd key, then re-verify the split */
	for (size_t i = 0; i < s->nkeys; i += 3) {
		char keyobj[SCENARIO_KEY_MAX];

		memset(keyobj, 0, sizeof(keyobj));
		s->fill(keyobj, i);

		mm_hashmap_keylock_t kl;
		int rc = mm_hashmap_lock_key(hm, &kl, keyobj, 0);
		test(rc == 0);
		test(kl.found == 1);

		mm_hashmap_remove(hm, &kl);
	}

	for (size_t i = 0; i < s->nkeys; i++) {
		char keyobj[SCENARIO_KEY_MAX];

		memset(keyobj, 0, sizeof(keyobj));
		s->fill(keyobj, i);

		mm_hashmap_keylock_t kl;
		int rc = mm_hashmap_lock_key(hm, &kl, keyobj, 0);
		test(rc == 0);
		test(kl.found == (i % 3 == 0) ? 0 : 1);
		if (kl.found) {
			test(*(int64_t *)mm_hashmap_kvp_val(hm, kl.kvp) ==
			     (int64_t)i);
		}
		mm_hashmap_unlock_key(hm, &kl);
	}

	/*
	 * with_hash consistency: a hash computed by the map's own
	 * khash must find entries inserted by the plain lock_key
	 * (this pins the "cached hash == khash" invariant for every
	 * production key shape); a hash differing in a masked bit
	 * must not find anything.  also pins the no-lock-held-on-miss
	 * contract of the no-create path.
	 */
	for (size_t i = 0; i < s->nkeys; i++) {
		char keyobj[SCENARIO_KEY_MAX];

		memset(keyobj, 0, sizeof(keyobj));
		s->fill(keyobj, i);

		mm_hash_t h = s->ctl.khash(keyobj);

		mm_hashmap_keylock_t kl;
		int rc = mm_hashmap_lock_key_with_hash(hm, &kl, keyobj, h, 0);
		test(rc == 0);
		test(kl.found == (i % 3 != 0));
		if (kl.found) {
			test(*(int64_t *)mm_hashmap_kvp_val(hm, kl.kvp) ==
			     (int64_t)i);
			mm_hashmap_unlock_key(hm, &kl);
		}

		rc = mm_hashmap_lock_key_with_hash(hm, &kl, keyobj, h ^ 1, 0);
		test(rc == 0);
		test(kl.found == 0);
	}

	/*
	 * lock_shard: no lookup, always locked on success, points at
	 * the bucket the key's hash selects; unlock via unlock_key
	 */
	for (size_t i = 0; i < s->nkeys; i += 11) {
		char keyobj[SCENARIO_KEY_MAX];

		memset(keyobj, 0, sizeof(keyobj));
		s->fill(keyobj, i);

		mm_hash_t h = s->ctl.khash(keyobj);

		mm_hashmap_keylock_t kl;
		int rc = mm_hashmap_lock_shard(hm, &kl, h);
		test(rc == 0);
		test(kl.found == 0);
		test(kl.kvp == NULL);
		test(kl.bucket == &hm->buckets[h & hm->bucket_mask]);
		mm_hashmap_unlock_key(hm, &kl);
	}

	/* distribution: count, max chain, occupancy */
	size_t remaining = s->nkeys - ((s->nkeys + 2) / 3);
	size_t cnt = 0;
	size_t max_chain = 0;
	size_t nonempty = 0;
	for (size_t b = 0; b < hm->nbuckets; b++) {
		size_t chain = 0;
		mm_list_t *i;

		mm_hashmap_bucket_t *bucket = &hm->buckets[b];
		mm_list_foreach (&bucket->kvps, i) {
			chain++;
		}
		if (chain > 0) {
			nonempty++;
		}
		if (chain > max_chain) {
			max_chain = chain;
		}
		cnt += chain;
	}

	test(cnt == remaining);
	test(max_chain < s->max_bound);

	/* removed load is alpha/3 ~0.5: 30-65% of buckets non-empty */
	double occupancy = (double)nonempty / (double)hm->nbuckets;
	test(occupancy > 0.3 && occupancy < 0.65);

	mm_hashmap_free(hm);
}

static const scenario_t scenarios[] = {
	/* clients_map.c: kiwi cancel keys, config-provided lock count.
	 * nlocks=13 rounds to 16 */
	{
		.name = "clients_map",
		.flags = MM_HM_NLOCKS,
		.nkeys = 24576,
		.nbuckets = 32768,
		.max_bound = 10,
		.fill = fill_clients_key,
		.ctl = {
			.nbuckets = 32768,
			.keysz = sizeof(kiwi_key_t),
			.valsz = sizeof(void *),
			.kcmp = clients_key_cmp,
			.khash = clients_key_hash,
			.nlocks = 13,
		},
	},
	/* pstmt.c:64 client pstmt shells, ptr keys + strdup copy */
	{
		.name = "client_pstmt",
		.flags = MM_HM_KEYDTOR | MM_HM_KEYCOPY | MM_HM_NFREE_MAX,
		.nkeys = 48,
		.nbuckets = 64,
		.max_bound = 8,
		.fill = fill_client_pstmt,
		.ctl = {
			.nbuckets = 50,
			.keysz = sizeof(char *),
			.valsz = sizeof(void *),
			.kcmp = str_ptr_cmp,
			.khash = str_ptr_hash,
			.kdtor = str_ptr_dtor,
			.kcopy = str_ptr_copy,
			.nfree_max = 64,
		},
	},
	/* pstmt.c:212 client portals, client-chosen ptr keys */
	{
		.name = "client_portal",
		.flags = MM_HM_KEYDTOR | MM_HM_KEYCOPY | MM_HM_NFREE_MAX,
		.nkeys = 48,
		.nbuckets = 64,
		.max_bound = 8,
		.fill = fill_client_portal,
		.ctl = {
			.nbuckets = 50,
			.keysz = sizeof(char *),
			.valsz = sizeof(void *),
			.kcmp = str_ptr_cmp,
			.khash = str_ptr_hash,
			.kdtor = str_ptr_dtor,
			.kcopy = str_ptr_copy,
			.nfree_max = 32,
		},
	},
	/* pstmt.c:359 server stmts, inplace names, 1 lock */
	{
		.name = "server_pstmt",
		.flags = MM_HM_NLOCKS,
		.nkeys = 768,
		.nbuckets = 1024,
		.max_bound = 12,
		.fill = fill_server_pstmt,
		.ctl = {
			.nbuckets = 1000,
			.keysz = sizeof(test_pstmt_name_t),
			.valsz = sizeof(void *),
			.kcmp = inplace_str_cmp,
			.khash = inplace_str_hash,
			.nlocks = 1,
		},
	},
	/* pstmt.c:682 global map, query-desc keys, config locks
	 * (nlocks=9 rounds to 16) */
	{
		.name = "global_pstmts",
		.flags = MM_HM_NLOCKS,
		.nkeys = 12288,
		.nbuckets = 16384,
		.max_bound = 10,
		.fill = fill_global_desc,
		.ctl = {
			.nbuckets = 10000,
			.keysz = sizeof(test_desc_t),
			.valsz = sizeof(void *),
			.kcmp = desc_cmp,
			.khash = desc_hash,
			.nlocks = 9,
		},
	},
	/* sql_pstmt.c:130 per-connection map, client-named ptr keys */
	{
		.name = "sql_pstmt",
		.flags = MM_HM_KEYDTOR | MM_HM_KEYCOPY,
		.nkeys = 48,
		.nbuckets = 64,
		.max_bound = 8,
		.fill = fill_sql_name,
		.ctl = {
			.nbuckets = 64,
			.keysz = sizeof(char *),
			.valsz = sizeof(void *),
			.kcmp = str_ptr_cmp,
			.khash = str_ptr_hash,
			.kdtor = str_ptr_dtor,
			.kcopy = str_ptr_copy,
		},
	},
};

static void test_hm_scenarios_coroutine(void *arg)
{
	(void)arg;

	for (size_t i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++) {
		run_scenario(&scenarios[i]);
	}

	machine_stop_current();
}

void machinarium_test_hm_dist(void)
{
	machinarium_init();

	/* scenarios with locks exercise mm_mutex: need a machine
	 * context */
	int id = machine_create("test_hm_dist", test_hm_scenarios_coroutine,
				NULL);
	test(id != -1);

	int rc = machine_wait(id);
	test(rc != -1);

	machinarium_free();
}
