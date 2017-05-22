#ifndef MM_CHANNEL_H_
#define MM_CHANNEL_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_channelrd_t mm_channelrd_t;
typedef struct mm_channel_t   mm_channel_t;

struct mm_channelrd_t {
	mm_call_t call;
	int       signaled;
	mm_list_t link;
};

struct mm_channel_t {
	mm_list_t incoming;
	int       incoming_count;
	mm_list_t readers;
	int       readers_count;
};

static inline void
mm_channel_init(mm_channel_t *channel)
{
	mm_list_init(&channel->incoming);
	channel->incoming_count = 0;
	mm_list_init(&channel->readers);
	channel->readers_count = 0;
}

static inline void
mm_channel_free(mm_channel_t *channel)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&channel->incoming, i, n) {
		mm_msg_t *msg;
		msg = mm_container_of(i, mm_msg_t, link);
		mm_msg_unref(&machinarium.msg_pool, msg);
	}
}

static inline void
mm_channel_write(mm_channel_t *channel, mm_msg_t *msg)
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

static inline mm_msg_t*
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

#endif
