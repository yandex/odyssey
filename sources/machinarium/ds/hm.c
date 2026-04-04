#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

#include <machinarium/macro.h>
#include <machinarium/ds/hm.h>
#include <machinarium/memory.h>
#include <machinarium/machine.h>

static size_t adjust_to_prime(size_t sz)
{
	static size_t primes[] = {
		3,	 7,	  17,	   23,	    29,	     37,      47,
		59,	 71,	  89,	   107,	    131,     163,     197,
		239,	 293,	  353,	   431,	    521,     631,     761,
		919,	 1103,	  1327,	   1597,    1931,    2333,    2801,
		3371,	 4049,	  4861,	   5839,    7013,    8419,    10103,
		12143,	 14591,	  17519,   21023,   25229,   30293,   36353,
		43627,	 52361,	  62851,   75431,   90523,   108631,  130363,
		156437,	 187751,  225307,  270371,  324449,  389357,  467237,
		560689,	 672827,  807403,  968897,  1162687, 1395263, 1674319,
		2009191, 2411033, 2893249, 3471899, 4166287, 4999559, 5999471,
		7199369
	};

	size_t np = sizeof(primes) / sizeof(size_t);

	for (size_t i = 0; i < np; ++i) {
		if (primes[i] >= sz) {
			return primes[i];
		}
	}

	return primes[np - 1];
}

static size_t bucket_idx(mm_hashmap_t *hm, mm_hash_t hash)
{
	return hash % hm->nbuckets;
}

static size_t lock_idx(mm_hashmap_t *hm, size_t bucket_idx)
{
	return bucket_idx % hm->nlocks;
}

static size_t alignsz(size_t sz)
{
	size_t align = alignof(max_align_t);
	return (sz + align - 1) & ~(align - 1);
}

static void init_bucket(mm_hashmap_bucket_t *b)
{
	mm_list_init(&b->kvps);
}

static void init_buckets(mm_hashmap_t *hm)
{
	for (size_t i = 0; i < hm->nbuckets; ++i) {
		init_bucket(&hm->buckets[i]);
	}
}

static void init_locks(mm_hashmap_t *hm)
{
	for (size_t i = 0; i < hm->nlocks; ++i) {
		mm_mutex_init(&hm->locks[i]);
	}
}

static void destroy_locks(mm_hashmap_t *hm)
{
	for (size_t i = 0; i < hm->nlocks; ++i) {
		mm_mutex_destroy(&hm->locks[i]);
	}
}

void *mm_hashmap_kvp_key(const mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp)
{
	return (void *)((uint8_t *)kvp + hm->keyoff);
}

void *mm_hashmap_kvp_val(const mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp)
{
	return (void *)((uint8_t *)kvp + hm->valoff);
}

const void *mm_hashmap_kvp_key_const(const mm_hashmap_t *hm,
				     const mm_hashmap_kvp_t *kvp)
{
	return (const void *)((const uint8_t *)kvp + hm->keyoff);
}

const void *mm_hashmap_kvp_val_const(const mm_hashmap_t *hm,
				     const mm_hashmap_kvp_t *kvp)
{
	return (const void *)((const uint8_t *)kvp + hm->valoff);
}

static mm_hashmap_kvp_t *kvp_create(mm_hashmap_t *hm, mm_hash_t hash,
				    const void *key)
{
	mm_hashmap_kvp_t *kvp = mm_malloc(hm->kvpsize);
	if (kvp == NULL) {
		return NULL;
	}
	memset(kvp, 0, hm->kvpsize);

	mm_list_init(&kvp->link);
	kvp->hash = hash;

	void *kkey = mm_hashmap_kvp_key(hm, kvp);
	if (hm->kcopy != NULL) {
		int rc = hm->kcopy(kkey, key);
		if (rc != 0) {
			mm_free(kvp);
			return NULL;
		}
	} else {
		memcpy(kkey, key, hm->keysz);
	}

	return kvp;
}

static void kvp_free(mm_hashmap_t *hm, mm_hashmap_kvp_t *kvp)
{
	if (hm->kdtor != NULL) {
		hm->kdtor(mm_hashmap_kvp_key(hm, kvp));
	}

	if (hm->vdtor != NULL) {
		hm->vdtor(mm_hashmap_kvp_val(hm, kvp));
	}

	mm_free(kvp);
}

mm_hashmap_t *mm_hashmap_create(size_t nbuckets, size_t nlocks, size_t keysz,
				size_t valsz, mm_hm_key_cmp_fn kcmp,
				mm_hm_key_hash_fn khash,
				mm_hm_key_dtor_fn kdtor,
				mm_hm_value_dtor_fn vdtor,
				mm_hm_key_copy_fn kcopy)
{
	nbuckets = adjust_to_prime(nbuckets);

	if (nlocks == 0) {
		nlocks = 1;
	}
	if (nlocks > 1) {
		nlocks = adjust_to_prime(nlocks);
	}

	if (nlocks > nbuckets) {
		nlocks = nbuckets;
	}

	mm_hashmap_t *hm = mm_malloc(sizeof(mm_hashmap_t));
	if (hm == NULL) {
		return NULL;
	}

	hm->buckets = mm_malloc(sizeof(mm_hashmap_bucket_t) * nbuckets);
	if (hm->buckets == NULL) {
		mm_free(hm);
		return NULL;
	}

	hm->locks = mm_malloc(sizeof(mm_mutex_t) * nlocks);
	if (hm->locks == NULL) {
		mm_free(hm->buckets);
		mm_free(hm);
		return NULL;
	}

	hm->nbuckets = nbuckets;
	hm->nlocks = nlocks;
	hm->keysz = keysz;
	hm->valsz = valsz;
	hm->kcmp = kcmp;
	hm->khash = khash;
	hm->kdtor = kdtor;
	hm->vdtor = vdtor;
	hm->kcopy = kcopy;

	hm->keyoff = alignsz(sizeof(mm_hashmap_kvp_t));
	hm->valoff = alignsz(hm->keyoff + hm->keysz);
	hm->kvpsize = alignsz(hm->valoff + hm->valsz);

	init_buckets(hm);
	init_locks(hm);

	return hm;
}

void mm_hashmap_clear(mm_hashmap_t *hm)
{
	mm_list_t freelist, *i, *s;
	mm_list_init(&freelist);

	for (size_t li = 0; li < hm->nlocks; ++li) {
		mm_mutex_lock2(&hm->locks[li]);
	}

	for (size_t bi = 0; bi < hm->nbuckets; ++bi) {
		mm_hashmap_bucket_t *b = &hm->buckets[bi];

		mm_list_foreach_safe (&b->kvps, i, s) {
			mm_hashmap_kvp_t *kvp =
				mm_container_of(i, mm_hashmap_kvp_t, link);
			mm_list_unlink(&kvp->link);
			mm_list_append(&freelist, &kvp->link);
		}
	}

	for (int li = (int)hm->nlocks - 1; li >= 0; --li) {
		mm_mutex_unlock(&hm->locks[li]);
	}

	mm_list_foreach_safe (&freelist, i, s) {
		mm_hashmap_kvp_t *kvp =
			mm_container_of(i, mm_hashmap_kvp_t, link);
		mm_list_unlink(&kvp->link);
		kvp_free(hm, kvp);
	}
}

void mm_hashmap_free(mm_hashmap_t *hm)
{
	if (hm == NULL) {
		return;
	}

	mm_hashmap_clear(hm);

	destroy_locks(hm);

	mm_free(hm->locks);
	mm_free(hm->buckets);
	mm_free(hm);
}

int mm_hashmap_foreach(mm_hashmap_t *hm, mm_hm_kvp_cb_fn cb, void **argv)
{
	int rc = 0;

	for (size_t bi = 0; bi < hm->nbuckets; ++bi) {
		mm_hashmap_bucket_t *b = &hm->buckets[bi];
		mm_mutex_t *mu = &hm->locks[lock_idx(hm, bi)];

		mm_mutex_lock2(mu);

		mm_list_t *i;
		mm_list_foreach (&b->kvps, i) {
			mm_hashmap_kvp_t *kvp =
				mm_container_of(i, mm_hashmap_kvp_t, link);

			rc = cb(kvp, argv);
			if (rc != 0) {
				break;
			}
		}

		mm_mutex_unlock(mu);

		if (rc != 0) {
			break;
		}
	}

	return rc;
}

static mm_hashmap_kvp_t *kvp_find_locked(mm_hashmap_t *hm,
					 mm_hashmap_bucket_t *bucket,
					 mm_hash_t hash, const void *key)
{
	mm_list_t *i;
	mm_list_foreach (&bucket->kvps, i) {
		mm_hashmap_kvp_t *kvp =
			mm_container_of(i, mm_hashmap_kvp_t, link);

		if (kvp->hash != hash) {
			continue;
		}

		void *kkey = mm_hashmap_kvp_key(hm, kvp);
		if (hm->kcmp(kkey, key) == 0) {
			return kvp;
		}
	}

	return NULL;
}

int mm_hashmap_lock_key(mm_hashmap_t *hm, mm_hashmap_keylock_t *klock,
			const void *key, int create)
{
	memset(klock, 0, sizeof(mm_hashmap_keylock_t));

	mm_hash_t hash = hm->khash(key);
	size_t bidx = bucket_idx(hm, hash);
	size_t lidx = lock_idx(hm, bidx);
	mm_mutex_t *mu = &hm->locks[lidx];
	mm_hashmap_bucket_t *bucket = &hm->buckets[bidx];

	mm_mutex_lock2(mu);
	klock->mu = mu;

	mm_hashmap_kvp_t *kvp = kvp_find_locked(hm, bucket, hash, key);
	if (kvp != NULL) {
		klock->found = 1;
		klock->kvp = kvp;
		return 0;
	}

	mm_mutex_unlock(mu);

	if (!create) {
		klock->found = 0;
		klock->kvp = NULL;
		return 0;
	}

	mm_hashmap_kvp_t *newkvp = kvp_create(hm, hash, key);
	if (newkvp == NULL) {
		return -1;
	}

	mm_mutex_lock2(mu);

	kvp = kvp_find_locked(hm, bucket, hash, key);
	if (kvp != NULL) {
		kvp_free(hm, newkvp);
		klock->kvp = kvp;
		klock->found = 1;
		return 0;
	}

	mm_list_append(&bucket->kvps, &newkvp->link);
	klock->kvp = newkvp;
	klock->found = 0;

	return 0;
}

void mm_hashmap_unlock_key(mm_hashmap_t *hm, mm_hashmap_keylock_t *klock)
{
	(void)hm;
	mm_mutex_unlock(klock->mu);
}

void mm_hashmap_remove(mm_hashmap_t *hm, mm_hashmap_keylock_t *klock)
{
	if (klock->kvp != NULL) {
		mm_list_unlink(&klock->kvp->link);
		kvp_free(hm, klock->kvp);
		klock->kvp = NULL;
	}

	mm_mutex_unlock(klock->mu);
}
