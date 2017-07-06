#ifndef SHAPITO_BE_READ_H
#define SHAPITO_BE_READ_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_be_startup shapito_be_startup_t;

struct shapito_be_startup
{
	int                   is_ssl_request;
	int                   is_cancel;
	shapito_key_t         key;
	shapito_parameters_t  params;
	shapito_parameter_t  *user;
	shapito_parameter_t  *database;
	shapito_parameter_t  *application_name;
};

static inline void
shapito_be_startup_init(shapito_be_startup_t *su)
{
	su->is_cancel = 0;
	su->is_ssl_request = 0;
	su->user = NULL;
	su->database = NULL;
	su->application_name = NULL;
	shapito_parameters_init(&su->params);
	shapito_key_init(&su->key);
}

static inline void
shapito_be_startup_free(shapito_be_startup_t *su)
{
	shapito_parameters_free(&su->params);
}

shapito_api int
shapito_be_read_startup(shapito_be_startup_t*, char *data, uint32_t size);

shapito_api int
shapito_be_read_password(shapito_password_t*, char *data, uint32_t size);

#endif /* SHAPITO_BE_READ_H */
