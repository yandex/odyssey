#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

/*
 * yet another hashmap implementation
 * this one is concurrent variant ready machinarium engine
 */

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

#include <machinarium/mutex.h>
#include <machinarium/list.h>
#include <machinarium/ds/hash.h>
#include <machinarium/ds/intrusive/lf-stack.h>

typedef struct mm_hashmap mm_hashmap_t;

typedef int (*mm_hm_key_cmp_fn)(const void *key1, const void *key2);
typedef mm_hash_t (*mm_hm_key_hash_fn)(const void *key);
typedef void (*mm_hm_key_dtor_fn)(void *key);
typedef void (*mm_hm_value_dtor_fn)(void *value);
typedef int (*mm_hm_key_copy_fn)(void *dst, const void *src);

typedef struct {
	/* while pooled in the freelist the kvp is not linked into any
	 * bucket, so the link storage may serve as the lf-stack entry */
	union {
		mm_list_t link;
		mm_lf_stack_entry_t stack;
	};
	mm_hash_t hash;
	uint8_t keyval[];
} mm_hashmap_kvp_t;

typedef int (*mm_hm_kvp_cb_fn)(mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp,
			       void **argv);

typedef struct {
	/* mm_hashmap_kvp_t[] */
	mm_list_t kvps;
	mm_mutex_t *mu;
} mm_hashmap_bucket_t;

typedef struct {
	mm_hashmap_kvp_t *kvp;
	mm_hashmap_bucket_t *bucket;
	int found;
} mm_hashmap_keylock_t;

typedef enum {
	MM_HASHMAP_CREATE = 1 << 0,
} mm_hashmap_lockkey_flags_t;

struct mm_hashmap {
	size_t keysz;
	size_t valsz;

	size_t keyoff;
	size_t valoff;
	size_t kvpsize;

	mm_hm_key_cmp_fn kcmp;
	mm_hm_key_hash_fn khash;
	mm_hm_key_dtor_fn kdtor;
	mm_hm_value_dtor_fn vdtor;
	mm_hm_key_copy_fn kcopy;

	size_t nbuckets;
	mm_hashmap_bucket_t *buckets;

	size_t nlocks;
	mm_mutex_t *locks;

	mm_lf_stack_t freelist;

	atomic_size_t nfree;
	size_t nfree_max;
};

void *mm_hashmap_kvp_key(const mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp);
void *mm_hashmap_kvp_val(const mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp);

const void *mm_hashmap_kvp_key_const(const mm_hashmap_t *hm,
				     const mm_hashmap_kvp_t *kvp);
const void *mm_hashmap_kvp_val_const(const mm_hashmap_t *hm,
				     const mm_hashmap_kvp_t *kvp);

/* nfree_max: freelist capacity in kvps; 0 = nbuckets (map-sized default) */
mm_hashmap_t *mm_hashmap_create(size_t nbuckets, size_t nlocks, size_t keysz,
				size_t valsz, mm_hm_key_cmp_fn kcmp,
				mm_hm_key_hash_fn khash,
				mm_hm_key_dtor_fn kdtor,
				mm_hm_value_dtor_fn vdtor,
				mm_hm_key_copy_fn kcopy, size_t nfree_max);
void mm_hashmap_free(mm_hashmap_t *hm);
void mm_hashmap_clear(mm_hashmap_t *hm);
int mm_hashmap_foreach(mm_hashmap_t *hm, mm_hm_kvp_cb_fn cb, void **argv);
int mm_hashmap_lock_key(mm_hashmap_t *hm, mm_hashmap_keylock_t *klock,
			const void *key,
			int flags /* see mm_hashmap_lockkey_flags_t */);
void mm_hashmap_unlock_key(mm_hashmap_t *hm, mm_hashmap_keylock_t *klock);
void mm_hashmap_remove(mm_hashmap_t *hm, mm_hashmap_keylock_t *klock);
