
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API machine_msg_t*
machine_msg_create(int type, int data_size)
{
	mm_msg_t *msg = mm_msgcache_pop(&machinarium.msg_cache);
	if (msg == NULL)
		return NULL;
	msg->type = type;
	if (data_size > 0) {
		int rc;
		rc = mm_buf_ensure(&msg->data, data_size);
		if (rc == -1) {
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

MACHINE_API void*
machine_msg_get_data(machine_msg_t *obj)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	return msg->data.start;
}

MACHINE_API int
machine_msg_get_type(machine_msg_t *obj)
{
	mm_msg_t *msg = mm_cast(mm_msg_t*, obj);
	return msg->type;
}
