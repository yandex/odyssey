#ifndef SHAPITO_PASSWORD_H
#define SHAPITO_PASSWORD_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_password shapito_password_t;

struct shapito_password
{
	char *password;
	int   password_len;
};

static inline void
shapito_password_init(shapito_password_t *pw)
{
	pw->password = NULL;
	pw->password_len = 0;
}

static inline void
shapito_password_free(shapito_password_t *pw)
{
	if (pw->password)
		free(pw->password);
}

static inline int
shapito_password_compare(shapito_password_t *a, shapito_password_t *b)
{
	return (a->password_len == b->password_len) &&
	       (memcmp(a->password, b->password, a->password_len) == 0);
}

uint32_t shapito_password_salt(shapito_key_t*);
int      shapito_password_md5(shapito_password_t*,
                              char *user, int user_len,
                              char *password, int password_len,
                              char salt[4]);

#endif /* SHAPITO_PASSWORD_H */
