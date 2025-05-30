#include "odyssey.h"
#include <odyssey_test.h>

#define OD_TEST_DEFAULT_HASHMAP_SZ 420u
#define OD_TEST_LARGE_HASHMAP_SZ 65534u

// Generate len-size string and safe to buf
void generate_random_string(char *buf, size_t len)
{
	static const char ALPHABET[] =
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./,\\?,<>";
	static const int LEN_ALPHABET = sizeof(ALPHABET) - 1;

	--len;
	for (size_t i = 0; i < len; i++) {
		buf[i] = ALPHABET[rand() % LEN_ALPHABET];
	}
	buf[len] = '\0';
}

// Save data in od_hashmap_elt_t, calc keyhash if need
static inline void test_hashmap_init_item(od_hashmap_elt_t *item, void *data,
					  size_t len, od_hash_t *keyhash)
{
	item->data = data;
	item->len = len;
	if (keyhash != NULL) {
		*keyhash = od_murmur_hash(item->data, item->len);
	}
}

// Add element to hashmap by key
void test_hashmap_add_element(od_hashmap_t *hm, void *key_data, size_t key_len,
			      void *value_data, size_t value_len)
{
	od_hashmap_elt_t key;
	od_hash_t keyhash;
	od_hashmap_elt_t *value;

	test_hashmap_init_item(&key, key_data, key_len, &keyhash);

	value = od_hashmap_lock_key(hm, keyhash, &key);

	test_hashmap_init_item(value, value_data, value_len, NULL);

	int rc;
	rc = od_hashmap_unlock_key(hm, keyhash, &key);
	test(rc == 0);
}

// Compare expected value and value by key
void test_hashmap_compare_element_by_key(od_hashmap_t *hm, void *key_data,
					 size_t key_len, void *value_data,
					 size_t value_len)
{
	od_hash_t keyhash;
	od_hashmap_elt_t key;
	od_hashmap_elt_t *ptr;

	test_hashmap_init_item(&key, key_data, key_len, &keyhash);

	ptr = od_hashmap_find(hm, keyhash, &key);

	test(ptr != NULL);
	test(ptr->len == value_len);
	test(memcmp(value_data, ptr->data, ptr->len) == 0);
}

// Small test to one item
void test_hashmap_one_item()
{
	od_hashmap_t *hm;

	hm = od_hashmap_create(OD_TEST_DEFAULT_HASHMAP_SZ);

	char key_data[] = "sqrt";

	char *value_data = malloc(12);
	strcpy(value_data, "192.168.1.1");

	test_hashmap_add_element(hm, key_data, 5, value_data, 12);
	test_hashmap_compare_element_by_key(hm, key_data, 5, value_data, 12);

	od_hashmap_free(hm);
}

// Initializing a hashmap of hm_size size and add hm_size elements of elemet_size sizes
void test_hashmap_many_items(const size_t hm_size, const size_t element_size)
{
	od_hashmap_t *hm;

	hm = od_hashmap_create(hm_size);

	char **strings = malloc(hm_size * sizeof(char *));
	size_t *keys = malloc(hm_size * sizeof(size_t));

	for (size_t i = 0; i < hm_size; ++i) {
		strings[i] = (char *)malloc(element_size);
		generate_random_string(strings[i], element_size);
		keys[i] = i;

		test_hashmap_add_element(hm, &keys[i], sizeof(i), strings[i],
					 element_size);
	}

	for (size_t i = 0; i < hm_size; ++i) {
		test_hashmap_compare_element_by_key(hm, &keys[i], sizeof(i),
						    strings[i], element_size);
	}

	od_hashmap_free(hm);
	free(keys);
	free(strings);
}

// Test insert method of hashmap
void test_hashmap_insert()
{
	od_hashmap_t *hm;

	hm = od_hashmap_create(OD_TEST_DEFAULT_HASHMAP_SZ);
	od_hash_t keyhash;
	od_hashmap_elt_t key;
	od_hashmap_elt_t value_1, value_2;
	od_hashmap_elt_t *value_ptr_1 = &value_1, *value_ptr_2 = &value_2;

	int rc;
	const size_t LEN = 40;
	char key_data[] = "sqrt";
	char *value_data_1 = malloc(LEN);
	generate_random_string(value_data_1, LEN);
	char *value_data_2 = malloc(LEN);
	generate_random_string(value_data_2, LEN);

	test_hashmap_init_item(&key, key_data, 5, &keyhash);
	test_hashmap_init_item(value_ptr_1, value_data_1, LEN, NULL);
	test_hashmap_init_item(value_ptr_2, value_data_2, LEN, NULL);

	rc = od_hashmap_insert(hm, keyhash, &key, &value_ptr_1);
	test(rc == 0);
	rc = od_hashmap_insert(hm, keyhash, &key, &value_ptr_2);
	test(rc == 1);

	od_hashmap_free(hm);
	free(value_data_1);
	free(value_data_2);
}

void odyssey_test_hashmap(void)
{
	srand(time(NULL));
	test_hashmap_one_item();
	test_hashmap_many_items(OD_TEST_DEFAULT_HASHMAP_SZ, 40);
	test_hashmap_many_items(OD_TEST_LARGE_HASHMAP_SZ, 100);
	test_hashmap_insert();
}