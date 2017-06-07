#ifndef SO_BEREAD_H_
#define SO_BEREAD_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_bestartup_t  so_bestartup_t;

struct so_bestartup_t {
	int              is_ssl_request;
	int              is_cancel;
	so_key_t         key;
	so_parameters_t  params;
	so_parameter_t  *user;
	so_parameter_t  *database;
	so_parameter_t  *application_name;
};

static inline void
so_bestartup_init(so_bestartup_t *su)
{
	su->is_cancel = 0;
	su->is_ssl_request = 0;
	su->user = NULL;
	su->database = NULL;
	su->application_name = NULL;
	so_parameters_init(&su->params);
	so_keyinit(&su->key);
}

static inline void
so_bestartup_free(so_bestartup_t *su)
{
	so_parameters_free(&su->params);
}

int so_beread_startup(so_bestartup_t*, uint8_t*, uint32_t);
int so_beread_password(so_password_t*, uint8_t*, uint32_t);

#endif
