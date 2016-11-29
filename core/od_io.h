#ifndef OD_READ_H_
#define OD_READ_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_read(mm_io_t, so_stream_t*, int);
int od_write(mm_io_t, so_stream_t*);

#endif
