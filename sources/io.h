#ifndef OD_IO_H
#define OD_IO_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

int od_read(machine_io_t*, shapito_stream_t*, int);
int od_write(machine_io_t*, shapito_stream_t*);
int od_getaddrname(struct addrinfo*, char*, int, int, int);
int od_getpeername(machine_io_t*, char*, int, int, int);
int od_getsockname(machine_io_t*, char*, int, int, int);

#endif /* OD_IO_H */
