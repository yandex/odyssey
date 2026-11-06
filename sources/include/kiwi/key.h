#pragma once

/*
 * kiwi.
 *
 * postgreSQL protocol interaction library.
 */

#include <stdint.h>
#include <string.h>

#define KIWI_CANCEL_KEY_MIN_LEN 4
#define KIWI_CANCEL_KEY_DEFAULT_LEN 32
#define KIWI_CANCEL_KEY_MAX_LEN 256

typedef struct kiwi_key kiwi_key_t;

struct kiwi_key {
	uint32_t key_pid;
	uint32_t key_len;
	/* Opaque bytes in wire order, including for a legacy four-byte key. */
	char key[KIWI_CANCEL_KEY_MAX_LEN];
};

static inline void kiwi_key_init(kiwi_key_t *key)
{
	memset(key, 0, sizeof(*key));
}

static inline void kiwi_key_set32(kiwi_key_t *key, uint32_t pid,
				  uint32_t secret)
{
	kiwi_key_init(key);
	key->key_pid = pid;
	key->key_len = KIWI_CANCEL_KEY_MIN_LEN;
	key->key[0] = (char)(secret >> 24);
	key->key[1] = (char)(secret >> 16);
	key->key[2] = (char)(secret >> 8);
	key->key[3] = (char)secret;
}

static inline int kiwi_key_cmp(const kiwi_key_t *a, const kiwi_key_t *b)
{
	return a->key_pid == b->key_pid && a->key_len == b->key_len &&
	       memcmp(a->key, b->key, a->key_len) == 0;
}
