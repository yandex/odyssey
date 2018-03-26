#ifndef SHAPITO_READ_H
#define SHAPITO_READ_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

SHAPITO_API int
shapito_read_startup(uint32_t *len, char **data, uint32_t *size);

SHAPITO_API int
shapito_read(uint32_t *len, char **data, uint32_t *size);

#endif /* SHAPITO_READ_H */
