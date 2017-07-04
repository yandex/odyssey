#ifndef SO_HEADER_H_
#define SO_HEADER_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct so_header so_header_t;

struct so_header
{
	uint8_t  type;
	uint32_t len;
	char     data[];
} so_packed;

#endif
