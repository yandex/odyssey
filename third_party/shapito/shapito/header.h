#ifndef SHAPITO_HEADER_H
#define SHAPITO_HEADER_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

typedef struct shapito_header shapito_header_t;

struct shapito_header
{
	uint8_t  type;
	uint32_t len;
	char     data[];
} shapito_packed;

#endif /* SHAPITO_HEADER_H */
