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
	uint8_t *severity;
	uint8_t *code;
	uint8_t *message;
	uint8_t *detail;
	uint8_t *hint;
};

int so_feread_ready(int*, uint8_t*, uint32_t);
int so_feread_key(so_key_t*, uint8_t*, uint32_t);
int so_feread_auth(uint32_t*, uint8_t[4], uint8_t*, uint32_t);
int so_feread_parameter(so_parameters_t*, uint8_t*, uint32_t);
int so_feread_error(so_feerror_t*, uint8_t*, uint32_t);

#endif
