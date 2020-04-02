
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <machinarium.h>
#include <machinarium_private.h>

MACHINE_API machine_channel_t *
machine_channel_create(int shared)
{
	if (shared) {
		mm_channel_t *channel;
		channel = malloc(sizeof(mm_channel_t));
		if (channel == NULL) {
			mm_errno_set(ENOMEM);
			return NULL;
		}
		mm_channel_init(channel);
		return (machine_channel_t *)channel;
	}

	mm_channelfast_t *channel;
	channel = malloc(sizeof(mm_channelfast_t));
	if (channel == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	mm_channelfast_init(channel);
	return (machine_channel_t *)channel;
}

MACHINE_API void
machine_channel_free(machine_channel_t *obj)
{
	mm_channeltype_t *type;
	type = mm_cast(mm_channeltype_t *, obj);
	if (type->is_shared) {
		mm_channel_t *channel;
		channel = mm_cast(mm_channel_t *, obj);
		mm_channel_free(channel);
		free(channel);
		return;
	}
	mm_channelfast_t *channel;
	channel = mm_cast(mm_channelfast_t *, obj);
	mm_channelfast_free(channel);
	free(channel);
}

MACHINE_API void
machine_channel_write(machine_channel_t *obj, machine_msg_t *obj_msg)
{
	mm_channeltype_t *type;
	type = mm_cast(mm_channeltype_t *, obj);
	if (type->is_shared) {
		mm_channel_t *channel;
		channel       = mm_cast(mm_channel_t *, obj);
		mm_msg_t *msg = mm_cast(mm_msg_t *, obj_msg);
		mm_channel_write(channel, msg);
		return;
	}
	mm_channelfast_t *channel;
	channel       = mm_cast(mm_channelfast_t *, obj);
	mm_msg_t *msg = mm_cast(mm_msg_t *, obj_msg);
	mm_channelfast_write(channel, msg);
}

MACHINE_API machine_msg_t *
machine_channel_read(machine_channel_t *obj, uint32_t time_ms)
{
	mm_channeltype_t *type;
	type = mm_cast(mm_channeltype_t *, obj);
	if (type->is_shared) {
		mm_channel_t *channel;
		channel = mm_cast(mm_channel_t *, obj);
		mm_msg_t *msg;
		msg = mm_channel_read(channel, time_ms);
		return (machine_msg_t *)msg;
	}
	mm_channelfast_t *channel;
	channel = mm_cast(mm_channelfast_t *, obj);
	mm_msg_t *msg;
	msg = mm_channelfast_read(channel, time_ms);
	return (machine_msg_t *)msg;
}
