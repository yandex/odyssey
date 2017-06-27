#ifndef MM_READ_H
#define MM_READ_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

int mm_read(mm_io_t*, char*, int, uint32_t);

int mm_readahead_start(mm_io_t*);
int mm_readahead_stop(mm_io_t*);

#endif /* MM_READ_H */
