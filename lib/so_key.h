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
so_key_init(sokey_t *key)
{
	key->key = 0;
	key->key_pid = 0;
}

#endif
