#ifndef SO_READ_H_
#define SO_READ_H_

/*
 * SHAPITO.
 *
 * Protocol-level PostgreSQL client library.
*/

int so_read_startup(uint32_t*, uint8_t**, uint32_t*);
int so_read(uint32_t*, uint8_t**, uint32_t*);

#endif
