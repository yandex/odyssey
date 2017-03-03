#ifndef SO_BEREAD_H_
#define SO_BEREAD_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_bestartup_t  so_bestartup_t;

struct so_bestartup_t {
	int       is_cancel;
	so_key_t  key;
	char     *database;
	int       database_len;
	char     *user;
	int       user_len;
	char     *application_name;
	int       application_name_len;
};

static inline void
so_bestartup_init(so_bestartup_t *su)
{
	su->is_cancel = 0;
	su->database = NULL;
	su->database_len = 0;
	su->user = NULL;
	su->user_len = 0;
	su->application_name = NULL;
	su->application_name_len = 0;
	so_keyinit(&su->key);
}

static inline void
so_bestartup_free(so_bestartup_t *su)
{
	if (su->database)
		free(su->database);
	if (su->user)
		free(su->user);
	if (su->application_name)
		free(su->application_name);
}

int so_beread_startup(so_bestartup_t*, uint8_t*, uint32_t);
int so_beread_password(so_password_t*, uint8_t*, uint32_t);

#endif
