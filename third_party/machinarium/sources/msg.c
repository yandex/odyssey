
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API machine_msg_t*
machine_msg_create(int data_size)
{
	mm_errno_set(0);
	mm_msg_t *msg = mm_msgcache_pop(&machinarium.msg_cache);
	if (msg == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	msg->type = 0;
	if (data_size > 0) {
		int rc;
		rc = mm_buf_ensure(&msg->data, data_size);
		if (rc == -1) {
			mm_errno_set(ENOMEM);
			mm_msg_unref(&machinarium.msg_cache, msg);
			return NULL;
		}
		mm_buf_advance(&msg->data, data_size);
	}
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
machine_msg_ensure(machine_msg_t *obj, int size)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	mm_errno_set(0);
	int rc = mm_buf_ensure(&msg->data, size);
	if (rc == -1) {
		mm_errno_set(ENOMEM);
		return -1;
	}
	return 0;
}

MACHINE_API void
machine_msg_write(machine_msg_t *obj, char *buf, int size)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	int rc = mm_buf_add(&msg->data, buf, size);
	(void)rc;
	assert(rc == 0);
}
