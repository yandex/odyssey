
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API machine_channel_t
machine_channel_create(void)
{
	mm_channel_t *channel;
	channel = malloc(sizeof(mm_channel_t));
	if (channel == NULL)
		return NULL;
	mm_channel_init(channel);
	return channel;
}

MACHINE_API void
machine_channel_free(machine_channel_t obj)
{
	mm_channel_t *channel = obj;
	mm_channel_free(channel);
	free(channel);
}

MACHINE_API void
machine_channel_write(machine_channel_t obj, machine_msg_t obj_msg)
{
	mm_channel_t *channel = obj;
	mm_msg_t *msg = obj_msg;
	mm_channel_write(channel, msg);
}

MACHINE_API machine_msg_t
machine_channel_read(machine_channel_t obj, int time_ms)
{
	mm_channel_t *channel = obj;
	mm_msg_t *msg;
	msg = mm_channel_read(channel, time_ms);
	return msg;
}
