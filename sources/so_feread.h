#ifndef SO_FEREAD_H_
#define SO_FEREAD_H_

/*
 * SHAPITO.
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
int so_feread_auth(uint32_t*, uint8_t[4], char*, uint32_t);
int so_feread_parameter(so_parameters_t*, char*, uint32_t);
int so_feread_error(so_feerror_t*, char*, uint32_t);

#endif
