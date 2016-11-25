#ifndef OD_READ_H_
#define OD_READ_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_read(mmio_t*, sostream_t*, int);
int od_write(mmio_t*, sostream_t*);

#endif
