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
	msg = machine_msg_create();
	if (msg == NULL)
		return -1;
	int rc;
	rc = machine_msg_write(msg, buf, size);
	if (rc == -1) {
		machine_msg_free(msg);
		return -1;
	}
	return mm_write(io, msg);
}

#endif /* MM_WRITE_H */
