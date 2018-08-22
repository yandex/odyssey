
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API machine_msg_t*
machine_msg_create(void)
{
	mm_errno_set(0);
	mm_msg_t *msg = mm_msgcache_pop(&machinarium.msg_cache);
	if (msg == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	msg->type = 0;
	return (machine_msg_t*)msg;
}

MACHINE_API void
machine_msg_free(machine_msg_t *obj)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	mm_msgcache_push(&machinarium.msg_cache, msg);
}

MACHINE_API void
machine_msg_set_type(machine_msg_t *obj, int type)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	msg->type = type;
}

MACHINE_API int
machine_msg_get_type(machine_msg_t *obj)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	return msg->type;
}

MACHINE_API void*
machine_msg_get_data(machine_msg_t *obj)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	return msg->data.start;
}

MACHINE_API int
machine_msg_get_size(machine_msg_t *obj)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	return mm_buf_used(&msg->data);
}

MACHINE_API int
machine_msg_write(machine_msg_t *obj, char *buf, int size)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	int rc;
	if (buf == NULL)
		rc = mm_buf_ensure(&msg->data, size);
	else
		rc = mm_buf_add(&msg->data, buf, size);
	if (rc == -1) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	return 0;
}
