#ifndef SO_PASSWORD_H_
#define SO_PASSWORD_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_password_t so_password_t;

struct so_password_t {
	char *password;
	int   password_len;
};

static inline void
so_password_init(so_password_t *pw)
{
	pw->password = NULL;
	pw->password_len = 0;
}

static inline void
so_password_free(so_password_t *pw)
{
	if (pw->password)
		free(pw->password);
}

static inline int
so_password_compare(so_password_t *a, so_password_t *b)
{
	return (a->password_len == b->password_len) &&
	       (memcmp(a->password, b->password, a->password_len) == 0);
}

#endif
