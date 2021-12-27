/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <kiwi.h>
#include <machinarium.h>
#include <odyssey.h>

od_hashmap_list_item_t *od_hashmap_list_item_create(void)
{
	od_hashmap_list_item_t *list;
	list = malloc(sizeof(od_hashmap_list_item_t));

	memset(list, 0, sizeof(od_hashmap_list_item_t));
	od_list_init(&list->link);
	return list;
}

void od_hashmap_list_item_add(od_hashmap_list_item_t *list,
			      const od_hashmap_list_item_t *it)
{
	od_list_append(&list->link, &it->link);
}

od_retcode_t od_hashmap_list_item_free(od_hashmap_list_item_t *l)
{
	od_list_unlink(&l->link);
	free(l->elt.data);
	free(l);

	return OK_RESPONSE;
}

static inline od_retcode_t od_hash_bucket_init(od_hashmap_bucket_t **b)
{
	*b = malloc(sizeof(od_hashmap_bucket_t));
	pthread_mutex_init(&(*b)->mu, NULL);
	(*b)->nodes = od_hashmap_list_item_create();

	return OK_RESPONSE;
}

od_hashmap_t *od_hashmap_create(size_t sz)
{
	od_hashmap_t *hm;

	hm = malloc(sizeof(od_hashmap_t));
	if (hm == NULL) {
		return NULL;
	}

	hm->size = sz;
	hm->buckets = malloc(sz * sizeof(od_hashmap_bucket_t *));

	if (hm->buckets == NULL) {
		free(hm);
		return NULL;
	}

	for (size_t i = 0; i < sz; ++i) {
		od_hash_bucket_init(&hm->buckets[i]);
	}

	return hm;
}

od_retcode_t od_hashmap_free(od_hashmap_t *hm)
{
	for (size_t i = 0; i < hm->size; ++i) {
		od_list_t *j, *n;

		od_list_foreach_safe(&hm->buckets[i]->nodes->link, j, n)
		{
		}

		pthread_mutex_destroy(&hm->buckets[i]->mu);
	}

	free(hm->buckets);
	free(hm);
}

static inline void *od_bucket_search(od_hashmap_bucket_t *b, void *key,
				     size_t key_len)
{
	od_list_t *i;
	od_list_foreach(&(b->nodes->link), i)
	{
		od_hashmap_list_item_t *item;
		item = od_container_of(i, od_hashmap_list_item_t, link);
		if (item->elt.len == key_len && memcmp(item->elt.data, key, key_len)) {
			// find
			return item->elt.data;
		}
	}

	return NULL;
}

od_retcode_t od_hashmap_insert(od_hashmap_t *hm, od_hash_t keyhash, void *key,
			       size_t key_len)
{
	size_t bucket_index = keyhash % hm->size;
	pthread_mutex_lock(&hm->buckets[bucket_index]->mu);

	void *ptr = od_bucket_search(hm->buckets[bucket_index], key, key_len);

	if (ptr == NULL) {
		od_hashmap_list_item_t *it;
		it = od_hashmap_list_item_create();
		it->elt.data = key;
		it->elt.len = key_len;
		od_hashmap_list_item_add(&hm->buckets[bucket_index], it);
	}

	pthread_mutex_unlock(&hm->buckets[bucket_index]->mu);
	return OK_RESPONSE;
}

void *od_hashmap_find(od_hashmap_t *hm, od_hash_t keyhash, void *key,
		      size_t key_len)
{
	size_t bucket_index = keyhash % hm->size;
	pthread_mutex_lock(&hm->buckets[bucket_index]->mu);

	void *ptr = od_bucket_search(hm->buckets[bucket_index], key, key_len);

	pthread_mutex_unlock(&hm->buckets[bucket_index]->mu);
	return ptr;
}
