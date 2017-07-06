#ifndef SHAPITO_FE_READ_H
#define SHAPITO_FE_READ_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_fe_error shapito_fe_error_t;

struct shapito_fe_error
{
	char *severity;
	char *code;
	char *message;
	char *detail;
	char *hint;
};

shapito_api int
shapito_fe_read_ready(int *status, char *data, uint32_t size);

shapito_api int
shapito_fe_read_key(shapito_key_t*, char *data, uint32_t size);

shapito_api int
shapito_fe_read_auth(uint32_t *type, char salt[4], char *data, uint32_t size);

shapito_api int
shapito_fe_read_parameter(shapito_parameters_t*, char *data, uint32_t size);

shapito_api int
shapito_fe_read_error(shapito_fe_error_t*, char *data, uint32_t size);

#endif /* SHAPITO_FE_READ_H */
