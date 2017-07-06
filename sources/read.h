#ifndef SHAPITO_READ_H
#define SHAPITO_READ_H

/*
 * Shapito.
 *
 * Protocol-level PostgreSQL client library.
*/

shapito_api int
shapito_read_startup(uint32_t *len, char **data, uint32_t *size);

shapito_api int
shapito_read(uint32_t *len, char **data, uint32_t *size);

#endif /* SHAPITO_READ_H */
