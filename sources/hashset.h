#ifndef OD_HASHSET_H
#define OD_HASHSET_H

typedef struct od_hashset_list od_hashset_list_t;

struct od_hashset_list {
	void *data;
	od_list_t *link;
};

extern od_hashset_list_t *od_hashset_list_create(void);

extern void od_hashset_list_add(od_hashset_list_t *list,
				const od_hashset_list_t *it);

extern od_retcode_t od_hashset_list_free(od_hashset_list_t *l);

#define OD_DEFAULT_HASH_TABLE_SIZE 15
typedef struct od_hashset_bucket {
	od_hashset_list_t *nodes;
	pthread_mutex_t mutex;
} od_hashset_bucket_t;

typedef struct od_hashset od_hashset_t;

struct od_hashset {
	size_t size;
	// ISO C99 flexible array member
	od_hashset_bucket_t *buckets[FLEXIBLE_ARRAY_MEMBER];
};
#endif /* OD_HASHSET_H */
