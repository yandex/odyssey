#ifndef SO_HEADER_H_
#define SO_HEADER_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_header_t so_header_t;

struct so_header_t {
	uint8_t  type;
	uint32_t len;
	uint8_t  data[];
} so_packed;

#endif
