#ifndef SO_KEY_H_
#define SO_KEY_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct sokey_t sokey_t;

struct sokey_t {
	uint32_t key;
	uint32_t key_pid;
};

static inline void
so_keyinit(sokey_t *key)
{
	key->key = 0;
	key->key_pid = 0;
}

static inline int
so_keycmp(sokey_t *a, sokey_t *b)
{
	return a->key == b->key &&
	       a->key_pid == b->key_pid;
}

#endif
