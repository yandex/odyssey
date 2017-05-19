
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API machine_msg_t
machine_msg_create(int type)
{
	mm_msg_t *msg = mm_msgpool_pop(&machinarium.msg_pool);
	if (msg == NULL)
		return NULL;
	msg->type = type;
	return msg;
}

MACHINE_API void
machine_msg_free(machine_msg_t obj)
{
	mm_msg_t *msg = obj;
	mm_msgpool_push(&machinarium.msg_pool, msg);
}

MACHINE_API void
machine_msg_set_data(machine_msg_t obj, void *data)
{
	mm_msg_t *msg = obj;
	msg->data = data;
}

MACHINE_API void*
machine_msg_get_data(machine_msg_t obj)
{
	mm_msg_t *msg = obj;
	return msg->data;
}

MACHINE_API int
machine_msg_get_type(machine_msg_t obj)
{
	mm_msg_t *msg = obj;
	return msg->type;
}
