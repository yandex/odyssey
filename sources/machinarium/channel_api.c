
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/channel.h>
#include <machinarium/machine.h>

MACHINE_API machine_channel_t *machine_channel_create()
{
	mm_channel_t *channel;
	channel = mm_malloc(sizeof(mm_channel_t));
	if (channel == NULL) {
		mm_errno_set(ENOMEM);
		return NULL;
	}
	mm_channel_init(channel);
	channel->limit_policy = MM_CHANNEL_UNLIMITED;
	return (machine_channel_t *)channel;
}

MACHINE_API void
machine_channel_assign_limit_policy(machine_channel_t *obj, int limit,
				    mm_channel_limit_policy_t policy)
{
	mm_channel_t *channel;
	channel = mm_cast(mm_channel_t *, obj);

	channel->chan_limit = limit;
	channel->limit_policy = policy;

	return;
}

MACHINE_API void machine_channel_free(machine_channel_t *obj)
{
	mm_channel_t *channel;
	channel = mm_cast(mm_channel_t *, obj);
	mm_channel_free(channel);
	mm_free(channel);
	return;
}

MACHINE_API mm_retcode_t machine_channel_write(machine_channel_t *obj,
					       machine_msg_t *obj_msg)
{
	mm_channel_t *channel;
	channel = mm_cast(mm_channel_t *, obj);
	mm_msg_t *msg = mm_cast(mm_msg_t *, obj_msg);
	return mm_channel_write(channel, msg);
}

MACHINE_API machine_msg_t *machine_channel_read(machine_channel_t *obj,
						uint32_t time_ms)
{
	mm_channel_t *channel;
	channel = mm_cast(mm_channel_t *, obj);
	mm_msg_t *msg;
	msg = mm_channel_read(channel, time_ms);
	return (machine_msg_t *)msg;
}

MACHINE_API machine_msg_t *machine_channel_read_back(machine_channel_t *obj,
						     uint32_t time_ms)
{
	mm_channel_t *channel;
	channel = mm_cast(mm_channel_t *, obj);
	mm_msg_t *msg;
	msg = mm_channel_read_back(channel, time_ms);
	return (machine_msg_t *)msg;
}

MACHINE_API size_t machine_channel_get_size(machine_channel_t *chan)
{
	return mm_channel_get_size(mm_cast(mm_channel_t *, chan));
}
