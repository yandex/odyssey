#ifndef ODYSSEY_IO_H
#define ODYSSEY_IO_H

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
*/

int od_getaddrname(struct addrinfo*, char*, int, int, int);
int od_getpeername(machine_io_t*, char*, int, int, int);
int od_getsockname(machine_io_t*, char*, int, int, int);

machine_msg_t*
od_read_startup(machine_io_t*, uint32_t);

machine_msg_t*
od_read(machine_io_t*, uint32_t);

#endif /* ODYSSEY_IO_H */
