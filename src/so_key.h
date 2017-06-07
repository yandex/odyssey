#ifndef SO_KEY_H_
#define SO_KEY_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_key_t so_key_t;

struct so_key_t {
	uint32_t key;
	uint32_t key_pid;
};

static inline void
so_keyinit(so_key_t *key)
{
	key->key = 0;
	key->key_pid = 0;
}

static inline int
so_keycmp(so_key_t *a, so_key_t *b)
{
	return a->key == b->key &&
	       a->key_pid == b->key_pid;
}

#endif
