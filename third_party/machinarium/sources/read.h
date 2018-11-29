#ifndef MM_READ_H
#define MM_READ_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

void mm_readahead_cb(mm_fd_t*);

static inline int
mm_readahead_enabled(mm_io_t *io)
{
	return io->readahead_size > 0;
}

int  mm_read_start(mm_io_t*, mm_fd_callback_t, void*);
int  mm_read_stop(mm_io_t*);
int  mm_read(mm_io_t*, char*, int, uint32_t);

#endif /* MM_READ_H */
