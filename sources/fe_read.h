#ifndef SHAPITO_FE_READ_H
#define SHAPITO_FE_READ_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_feerror so_feerror_t;

struct so_feerror
{
	char *severity;
	char *code;
	char *message;
	char *detail;
	char *hint;
};

int so_feread_ready(int*, char*, uint32_t);
int so_feread_key(so_key_t*, char*, uint32_t);
int so_feread_auth(uint32_t*, char[4], char*, uint32_t);
int so_feread_parameter(so_parameters_t*, char*, uint32_t);
int so_feread_error(so_feerror_t*, char*, uint32_t);

#endif /* SHAPITO_FE_READ_H */
