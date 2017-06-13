#ifndef OD_IO_H
#define OD_IO_H

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_read(machine_io_t*, so_stream_t*, int);
int od_write(machine_io_t*, so_stream_t*);
int od_getpeername(machine_io_t*, char*, int);

#endif /* OD_IO_H */
