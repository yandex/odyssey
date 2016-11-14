#ifndef OD_READ_H_
#define OD_READ_H_

/*
 * odissey.
 *
 * PostgreSQL connection pooler and request router.
*/

int od_read(ftio_t*, sostream_t*);
int od_write(ftio_t*, sostream_t*);

#endif
