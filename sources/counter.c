
#include "counter.h"

inline od_counter_llist_t *
od_counter_llist_create(void)
{
	od_counter_llist_t *llist = malloc(sizeof(od_counter_llist_t));
	if (llist == NULL)
		return NULL;

	llist->list  = NULL;
	llist->count = 0;

	return llist;
}

inline void
od_counter_llist_add(od_counter_llist_t *llist, const od_counter_item_t *it)
{
	od_counter_litem_t *litem = malloc(sizeof(od_counter_litem_t));
	if (litem == NULL)
		return;
	litem->value = *it;
	litem->cnt   = 1;

	litem->next = llist->list;
	llist->list = litem;

	++llist->count;
}

inline od_retcode_t
od_counter_llist_free(od_counter_llist_t *l)
{
	od_counter_litem_t *next = NULL;

	for (od_counter_litem_t *it = l->list; it != NULL; it = next) {
		next = it->next;
		free(it);
	}

	return OK_RESPONSE;
}

od_counter_t *
od_counter_create(size_t sz)
{
	od_counter_t *t = malloc(sizeof(od_counter_t));
	if (t == NULL) {
		return NULL;
	}
	t->buckets = malloc(sizeof(od_counter_llist_t) * sz);
	if (t->buckets == NULL) {
		return NULL;
	}
	t->bucket_mutex = malloc(sizeof(pthread_mutex_t) * sz);
	if (t->bucket_mutex == NULL) {
		free(t);
		return NULL;
	}
	t->size = sz;

	for (size_t i = 0; i < t->size; ++i) {
		t->buckets[i] = od_counter_llist_create();
		if (t->buckets[i] == NULL) {
			free(t->bucket_mutex);
			free(t);
			return NULL;
		}
		const int res = pthread_mutex_init(&t->bucket_mutex[i], NULL);
		if (res) {
			return NULL;
		}
	}

	return t;
}

od_counter_t *
od_counter_create_default(void)
{
	return od_counter_create(OD_DEFAULT_HASH_TABLE_SIZE);
}

od_retcode_t
od_counter_free(od_counter_t *t)
{
	for (size_t i = 0; i < t->size; ++i) {
		od_counter_llist_free(t->buckets[i]);
		pthread_mutex_destroy(&t->bucket_mutex[i]);
	}

	free(t->buckets);
	free(t);

	return OK_RESPONSE;
}

void
od_counter_inc(od_counter_t *t, od_counter_item_t item)
{
	od_counter_item_t key = od_hash_item(t, item);
	/*
	 * prevent concurrent access to
	 * modify hash table section
	 */
	pthread_mutex_lock(&t->bucket_mutex[key]);
	{
		bool fnd = false;

		for (od_counter_litem_t *it = t->buckets[key]->list; it != NULL;
		     it                     = it->next) {
			if (it->value == item) {
				++it->cnt;
				fnd = true;
				break;
			}
		}
		if (!fnd)
			od_counter_llist_add(t->buckets[key], &item);
	}
	pthread_mutex_unlock(&t->bucket_mutex[key]);
}

od_count_t
od_counter_get_count(od_counter_t *t, od_counter_item_t value)
{
	od_counter_item_t key = od_hash_item(t, value);

	od_count_t ret_val = 0;

	pthread_mutex_lock(&t->bucket_mutex[key]);
	{
		for (od_counter_litem_t *it = t->buckets[key]->list; it != NULL;
		     it                     = it->next) {
			if (it->value == value) {
				ret_val = it->cnt;
				break;
			}
		}
	}
	pthread_mutex_unlock(&t->bucket_mutex[key]);

	return ret_val;
}

static inline od_retcode_t
od_counter_reset_target_bucket(od_counter_t *t, size_t bucket_key)
{
	pthread_mutex_lock(&t->bucket_mutex[bucket_key]);
	{
		for (od_counter_litem_t *it = t->buckets[bucket_key]->list; it != NULL;
		     it                     = it->next) {
			it->value = 0;
		}
	}
	pthread_mutex_unlock(&t->bucket_mutex[bucket_key]);

	return OK_RESPONSE;
}

od_retcode_t
od_counter_reset(od_counter_t *t, od_counter_item_t value)
{
	od_counter_item_t key = od_hash_item(t, value);

	pthread_mutex_lock(&t->bucket_mutex[key]);
	{
		for (od_counter_litem_t *it = t->buckets[key]->list; it != NULL;
		     it                     = it->next) {
			if (it->value == value) {
				it->value = 0;
				break;
			}
		}
	}
	pthread_mutex_unlock(&t->bucket_mutex[key]);
	return OK_RESPONSE;
}

od_retcode_t
od_counter_reset_all(od_counter_t *t)
{
	for (size_t i = 0; i < t->size; ++i) {
		od_counter_reset_target_bucket(t, i);
	}
	return OK_RESPONSE;
}
