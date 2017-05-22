
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_channel_init(mm_channel_t *channel)
{
	mm_list_init(&channel->incoming);
	channel->incoming_count = 0;
	mm_list_init(&channel->readers);
	channel->readers_count = 0;
}

void mm_channel_free(mm_channel_t *channel)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&channel->incoming, i, n) {
		mm_msg_t *msg;
		msg = mm_container_of(i, mm_msg_t, link);
		mm_msg_unref(&machinarium.msg_pool, msg);
	}
}

void mm_channel_write(mm_channel_t *channel, mm_msg_t *msg)
{
	mm_list_append(&channel->incoming, &msg->link);
	channel->incoming_count++;

	if (! channel->readers_count)
		return;

	mm_list_t *first;
	first = channel->readers.next;
	mm_channelrd_t *reader;
	reader = mm_container_of(first, mm_channelrd_t, link);
	reader->signaled = 1;

	/* remove reader from the queue, to properly handle
	 * other waiting consumers */
	mm_list_unlink(&reader->link);
	channel->readers_count--;

	mm_scheduler_wakeup(&mm_self->scheduler, reader->call.fiber);
}

mm_msg_t*
mm_channel_read(mm_channel_t *channel, int time_ms)
{
	if (channel->incoming_count > 0)
		goto fetch;

	mm_channelrd_t reader;
	reader.signaled = 0;
	mm_list_init(&reader.link);

	mm_list_append(&channel->readers, &reader.link);
	channel->readers_count++;

	mm_call(&reader.call, time_ms);
	if (reader.call.status != 0) {
		/* timedout or cancel */
		if (! reader.signaled) {
			assert(channel->readers_count > 0);
			channel->readers_count--;
			mm_list_unlink(&reader.link);
		}
		return NULL;
	}
	assert(reader.signaled);

fetch:;
	mm_list_t *first;
	first = mm_list_pop(&channel->incoming);
	channel->incoming_count--;
	return mm_container_of(first, mm_msg_t, link);
}

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
