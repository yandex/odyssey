/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <errno.h>

#include <machinarium/machinarium.h>
#include <machinarium/memory.h>
#include <machinarium/util.h>
#include <machinarium/machine.h>
#include <machinarium/thread_pool.h>

mm_future_t *mm_future_create(mm_thread_pool_result_dtor_t dtor)
{
	mm_future_t *f = mm_malloc(sizeof(mm_future_t));
	if (f == NULL) {
		return f;
	}
	memset(f, 0, sizeof(mm_future_t));

	atomic_init(&f->value, (uintptr_t)NULL);
	atomic_init(&f->refs, 1);

	f->value_dtor = dtor;
	f->wait = mm_wait_flag_create();
	if (f->wait == NULL) {
		mm_free(f);
		return NULL;
	}

	return f;
}

void mm_future_ref(mm_future_t *future)
{
	atomic_fetch_add(&future->refs, 1);
}

void mm_future_unref(mm_future_t *future)
{
	uint64_t r = atomic_fetch_sub(&future->refs, 1);
	if (r == 1) {
		void *value = mm_future_get_result(future);
		if (future->value_dtor != NULL && value != NULL) {
			future->value_dtor(value);
		}

		mm_wait_flag_destroy(future->wait);
		mm_free(future);
	}
}

void *mm_future_get_result(mm_future_t *future)
{
	void *v = (void *)atomic_load(&future->value);
	return v;
}

static void task_destroy(void *t)
{
	mm_thread_pool_task_t *task = t;
	if (task->future != NULL) {
		mm_future_unref(task->future);
	}
}

static void tp_work(void *arg)
{
	mm_thread_pool_worker_t *worker = arg;
	mm_thread_pool_t *pool = worker->pool;

	while (1) {
		uint64_t version = atomic_load(&pool->version);

		mm_thread_pool_task_t task;
		if (mm_queue_pop(&pool->queue, &task)) {
			if (task.fn == NULL) {
				break;
			}

			if (task.detached ||
			    atomic_load(&task.future->refs) > 1) {
				/*
				 * the user might have give up waiting the task
				 * lets execute task only if user is awaiting the result
				 */
				void *result = task.fn(task.arg);
				atomic_store(&task.future->value,
					     (uintptr_t)result);
				mm_wait_flag_set(task.future->wait);
			}

			if (task.arg_dtor) {
				task.arg_dtor(task.arg);
			}
			mm_future_unref(task.future);

			continue;
		}

		mm_wait_list_compare_wait(&pool->notifier, NULL, version, 1000);
	}
}

static void notify_one(mm_thread_pool_t *pool)
{
	atomic_fetch_add(&pool->version, 1);
	mm_wait_list_notify(&pool->notifier);
}

static void stop(mm_thread_pool_t *pool)
{
	uint64_t exp = 0;
	if (!atomic_compare_exchange_strong(&pool->stop, &exp, 1)) {
		return;
	}

	for (size_t i = 0; i < pool->size; ++i) {
		mm_thread_pool_task_t cl;
		cl.future = NULL;
		cl.arg = NULL;
		cl.fn = NULL;

		while (!mm_queue_push(&pool->queue, &cl)) {
			/*
			 * TODO: do it better?
			 *
			 * no place in queue to submit the close task
			 * just wait for slot
			 */
			machine_sleep(1);
		}

		notify_one(pool);
	}
}

int mm_thread_pool_init(mm_thread_pool_t *pool, const char *name, size_t size,
			size_t queue_size)
{
	memset(pool, 0, sizeof(mm_thread_pool_t));

	strncpy(pool->name, name, sizeof(pool->name) - 1);

	int rc = mm_queue_init(&pool->queue, queue_size,
			       sizeof(mm_thread_pool_task_t), task_destroy);
	if (rc != 0) {
		return rc;
	}

	pool->size = size;
	atomic_init(&pool->version, 0);
	atomic_init(&pool->stop, 0);
	mm_wait_list_init(&pool->notifier, &pool->version);

	pool->workers = mm_malloc(size * sizeof(mm_thread_pool_worker_t));
	if (pool->workers == NULL) {
		mm_wait_list_destroy(&pool->notifier);
		mm_queue_destroy(&pool->queue);
		return -1;
	}
	memset(pool->workers, 0, size * sizeof(mm_thread_pool_worker_t));

	for (size_t i = 0; i < size; ++i) {
		mm_thread_pool_worker_t *wrk = &pool->workers[i];

		wrk->pool = pool;
		wrk->idx = i;

		mm_snprintf(wrk->name, sizeof(wrk->name), "%s_%lu", name, i);

		wrk->machine_id = machine_create(wrk->name, tp_work, wrk);
		if (wrk->machine_id == -1) {
			pool->size = i;
			mm_thread_pool_destroy(pool);
			return -1;
		}
	}

	return 0;
}

void mm_thread_pool_destroy(mm_thread_pool_t *pool)
{
	stop(pool);

	for (size_t j = 0; j < pool->size; ++j) {
		machine_wait(pool->workers[j].machine_id);
	}

	mm_free(pool->workers);
	mm_queue_destroy(&pool->queue);
	mm_wait_list_destroy(&pool->notifier);
}

mm_future_t *mm_thread_pool_submit(mm_thread_pool_t *pool,
				   mm_thread_pool_task_fn_t fn, void *arg,
				   mm_thread_pool_task_arg_dtor_t arg_dtor,
				   mm_thread_pool_result_dtor_t result_dtor,
				   int detach)
{
	if (atomic_load(&pool->stop)) {
		mm_errno_set(ENOTCONN);
		return NULL;
	}

	mm_future_t *future = mm_future_create(result_dtor);
	if (future == NULL) {
		return NULL;
	}

	mm_thread_pool_task_t task;
	memset(&task, 0, sizeof(mm_thread_pool_task_t));
	task.future = future;
	task.fn = fn;
	task.arg = arg;
	task.detached = detach;
	task.arg_dtor = arg_dtor;

	/* one another ref for 'user' */
	mm_future_ref(future);

	if (mm_queue_push(&pool->queue, &task)) {
		notify_one(pool);
		return future;
	}

	/* no actual 'user' exists, so double-unref */
	mm_future_unref(future);
	mm_future_unref(future);

	mm_errno_set(EAGAIN);
	return NULL;
}

int mm_thread_pool_wait(mm_future_t *future, uint32_t timeout_ms)
{
	int rc = mm_wait_flag_wait(future->wait, timeout_ms);

	return rc;
}
