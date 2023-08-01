#ifndef OD_HASHMAP_H
#define OD_HASHMAP_H

typedef struct od_hashmap_list_item od_hashmap_list_item_t;

// void * data should have following fmt:
// key
// value

// header, first keylen bytes is key, other is value
typedef struct {
	void *data;
	size_t len;
} od_hashmap_elt_t;

struct od_hashmap_list_item {
	od_hashmap_elt_t key;
	od_hashmap_elt_t value;
	od_list_t link;
};

extern od_hashmap_list_item_t *od_hashmap_list_item_create(void);

extern void od_hashmap_list_item_add(od_hashmap_list_item_t *list,
				     od_hashmap_list_item_t *it);

extern od_retcode_t od_hashmap_list_item_free(od_hashmap_list_item_t *l);

typedef struct od_hashmap_bucket {
	od_hashmap_list_item_t *nodes;
	pthread_mutex_t mu;
} od_hashmap_bucket_t;

typedef struct od_hashmap od_hashmap_t;

struct od_hashmap {
	size_t size;
	// ISO C99 flexible array member
	od_hashmap_bucket_t **buckets;
};

extern od_hashmap_t *od_hashmap_create(size_t sz);
extern od_retcode_t od_hashmap_free(od_hashmap_t *hm);
od_hashmap_elt_t *od_hashmap_find(od_hashmap_t *hm, od_hash_t keyhash,
				  od_hashmap_elt_t *key);

/* This function insert new key into hashmap
* If hashmap already contains value assotiated with key, 
* it will be rewritten.
*/
int od_hashmap_insert(od_hashmap_t *hm, od_hash_t keyhash,
		      od_hashmap_elt_t *key, od_hashmap_elt_t **value);

/* LOCK-UNLOCK API */
/* given key and its 
* keyhash (murmurhash etc) return poitner 
* to hashmap mutex-locked value pointer  */
od_hashmap_elt_t *od_hashmap_lock_key(od_hashmap_t *hm, od_hash_t keyhash,
				      od_hashmap_elt_t *key);

int od_hashmap_unlock_key(od_hashmap_t *hm, od_hash_t keyhash,
			  od_hashmap_elt_t *key);

/* clear hashmap */
od_retcode_t od_hashmap_empty(od_hashmap_t *hm);

#endif /* OD_HASHMAP_H */
