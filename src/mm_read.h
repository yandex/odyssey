#ifndef MM_READ_H_
#define MM_READ_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

int mm_read(mm_io_t*, char*, int, uint64_t);

int mm_readahead_start(mm_io_t*);
int mm_readahead_stop(mm_io_t*);

#endif
