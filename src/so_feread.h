#ifndef SO_FEREAD_H_
#define SO_FEREAD_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_feerror_t so_feerror_t;

struct so_feerror_t {
	uint8_t *severity;
	uint32_t severity_len;
	uint8_t *code;
	uint32_t code_len;
	uint8_t *message;
	uint32_t message_len;
};

int so_feread_ready(int*, uint8_t*, uint32_t);
int so_feread_key(so_key_t*, uint8_t*, uint32_t);
int so_feread_auth(uint32_t*, uint8_t[4], uint8_t*, uint32_t);
int so_feread_parameter(so_parameters_t*, uint8_t*, uint32_t);
int so_feread_error(so_feerror_t*, uint8_t*, uint32_t);

#endif
