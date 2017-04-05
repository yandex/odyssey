#ifndef OD_READ_H_
#define OD_READ_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_read(machine_io_t, so_stream_t*, int);
int od_write(machine_io_t, so_stream_t*);

char *od_getpeername(machine_io_t);

#endif
