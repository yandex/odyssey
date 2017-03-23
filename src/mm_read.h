#ifndef MM_READ_H_
#define MM_READ_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

static inline void
mm_io_read_stop(mm_io_t *io)
{
	if (uv_is_active((uv_handle_t*)&io->handle))
		uv_read_stop((uv_stream_t*)&io->handle);
}

#endif
