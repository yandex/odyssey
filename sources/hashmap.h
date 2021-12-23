#ifndef OD_HASHMAP_H
#define OD_HASHMAP_H

typedef struct od_hashmap_list od_hashmap_list_t;

struct od_hashmap_list {
	void *data;
	od_list_t *link;
};

extern od_hashmap_list_t *od_hashmap_list_create(void);

extern void od_hashmap_list_add(od_hashmap_list_t *list,
				 const od_hashmap_list_t *it);

extern od_retcode_t od_hashmap_list_free(od_hashmap_list_t *l);

#define OD_DEFAULT_HASH_TABLE_SIZE 15
typedef struct od_hashmap_bucket {
	od_hashmap_list_t *nodes;
	pthread_mutex_t mutex;
} od_hashmap_bucket_t;

typedef struct od_hashmap od_hashmap_t;

struct od_hashmap {
	size_t size;
	// ISO C99 flexible array member
	od_hashmap_bucket_t *buckets[FLEXIBLE_ARRAY_MEMBER];
};

#endif /* OD_HASHMAP_H */
