#ifndef SO_BEREAD_H_
#define SO_BEREAD_H_

/*
 * soprano.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_bestartup_t so_bestartup_t;

struct so_bestartup_t {
	int       is_cancel;
	so_key_t  key;
	char     *database;
	int       database_len;
	char     *user;
	int       user_len;
};

void so_bestartup_init(so_bestartup_t*);
void so_bestartup_free(so_bestartup_t*);

int  so_beread_startup(so_bestartup_t*, uint8_t*, uint32_t);

#endif
