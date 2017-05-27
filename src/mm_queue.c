
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_queue_init(mm_queue_t *queue)
{
	pthread_spin_init(&queue->lock, PTHREAD_PROCESS_PRIVATE);

	mm_list_init(&queue->msg_list);
	queue->msg_list_count = 0;

	mm_list_init(&queue->readers);
	queue->readers_count = 0;
}

void mm_queue_free(mm_queue_t *queue)
{
	mm_list_t *i, *n;
	mm_list_foreach_safe(&queue->msg_list, i, n) {
		mm_msg_t *msg = mm_container_of(i, mm_msg_t, link);
		mm_msg_unref(&machinarium.msg_cache, msg);
	}
	pthread_spin_destroy(&queue->lock);
}

void mm_queue_put(mm_queue_t *queue, mm_msg_t *msg)
{
	pthread_spin_lock(&queue->lock);

	if (queue->readers_count) {
		mm_queuerd_t *reader;
		reader = mm_container_of(queue->readers.next, mm_queuerd_t, link);
		reader->result = msg;

		mm_list_unlink(&reader->link);
		queue->readers_count--;

		mm_condition_signal(reader->condition);

		pthread_spin_unlock(&queue->lock);
		return;
	}

	mm_list_append(&queue->msg_list, &msg->link);
	queue->msg_list_count++;

	pthread_spin_unlock(&queue->lock);
}

mm_msg_t*
mm_queue_get(mm_queue_t *queue, mm_condition_t *condition, uint32_t time_ms)
{
	/* try to get first message, if no other readers are
	 * waiting, otherwise put reader in the wait
	 * queue */
	pthread_spin_lock(&queue->lock);

	mm_list_t *next;
	if (queue->msg_list_count > 0 && queue->readers_count == 0)
	{
		next = mm_list_pop(&queue->msg_list);
		queue->msg_list_count--;
		pthread_spin_unlock(&queue->lock);
		return mm_container_of(next, mm_msg_t, link);
	}

	mm_queuerd_t reader;
	reader.condition = condition;
	reader.result = NULL;
	mm_list_init(&reader.link);

	mm_list_append(&queue->readers, &reader.link);
	queue->readers_count++;

	pthread_spin_unlock(&queue->lock);

	/* put reader into sleep until a cancel, timedout
	 * or reader event happened */
	int status;
	status = mm_condition_wait(condition, time_ms);

	pthread_spin_lock(&queue->lock);

	if (! reader.result) {
		assert(queue->readers_count > 0);
		queue->readers_count--;
		mm_list_unlink(&reader.link);
	}

	pthread_spin_unlock(&queue->lock);

	/* timedout or cancel event */
	if (status != 0) {
		if (reader.result)
			mm_msg_unref(&machinarium.msg_cache, reader.result);
		return NULL;
	}

	/* on_read event */
	return reader.result;
}

MACHINE_API machine_queue_t
machine_queue_create(void)
{
	mm_queue_t *queue;
	queue = malloc(sizeof(mm_queue_t));
	if (queue == NULL)
		return NULL;
	mm_queue_init(queue);
	return queue;
}

MACHINE_API void
machine_queue_free(machine_queue_t obj)
{
	mm_queue_t *queue = obj;
	mm_queue_free(queue);
	free(queue);
}

MACHINE_API void
machine_queue_put(machine_queue_t obj, machine_msg_t obj_msg)
{
	mm_queue_t *queue = obj;
	mm_msg_t *msg = obj_msg;
	mm_queue_put(queue, msg);
}

MACHINE_API machine_msg_t
machine_queue_get(machine_queue_t obj, uint32_t time_ms)
{
	mm_queue_t *queue = obj;
	mm_condition_t *condition;
	condition  = mm_condition_cache_pop(&mm_self->condition_cache);
	if (condition == NULL)
		return NULL;
	mm_msg_t *msg;
	msg = mm_queue_get(queue, condition, time_ms);
	mm_condition_cache_push(&mm_self->condition_cache, condition);
	return msg;
}
