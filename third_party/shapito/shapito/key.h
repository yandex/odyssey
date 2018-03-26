#ifndef SHAPITO_KEY_H
#define SHAPITO_KEY_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_key shapito_key_t;

struct shapito_key
{
	uint32_t key;
	uint32_t key_pid;
};

static inline void
shapito_key_init(shapito_key_t *key)
{
	key->key = 0;
	key->key_pid = 0;
}

static inline int
shapito_key_cmp(shapito_key_t *a, shapito_key_t *b)
{
	return a->key == b->key &&
	       a->key_pid == b->key_pid;
}

#endif /* SHAPITO_KEY_H */
