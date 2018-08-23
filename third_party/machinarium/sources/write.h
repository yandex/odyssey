#ifndef MM_WRITE_H
#define MM_WRITE_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

int mm_write(mm_io_t*, machine_msg_t*);

static inline int
mm_write_buf(mm_io_t *io, char *buf, int size)
{
	machine_msg_t *msg;
	msg = machine_msg_create(size);
	if (msg == NULL)
		return -1;
	memcpy(machine_msg_get_data(msg), buf, size);
	return mm_write(io, msg);
}

#endif /* MM_WRITE_H */
