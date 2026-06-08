/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>

#include <machinarium/machine.h>

#include <util.h>
#include <logger.h>
#include <od_memory.h>
#include <thread_pool.h>

od_future_t *od_future_create(void)
{
	od_future_t *f = od_malloc(sizeof(od_future_t));
	if (f == NULL) {
		return f;
	}
	memset(f, 0, sizeof(od_future_t));

	atomic_init(&f->value, (uintptr_t)NULL);
	atomic_init(&f->refs, 1);

	f->wait = mm_wait_flag_create();
	if (f->wait == NULL) {
		od_free(f);
		return NULL;
	}

	return f;
}

void od_future_ref(od_future_t *future)
{
	atomic_fetch_add(&future->refs, 1);
}

void od_future_unref(od_future_t *future)
{
	uint64_t r = atomic_fetch_sub(&future->refs, 1);
	if (r == 1) {
		mm_wait_flag_destroy(future->wait);
		od_free(future);
	}
}

void *od_future_get_result(od_future_t *future)
{
	void *v = (void *)atomic_load(&future->value);
	return v;
}

static void task_destroy(void *t)
{
	od_thread_pool_task_t *task = t;
	if (task->future != NULL) {
		od_future_unref(task->future);
	}
}

static void tp_work(void *arg)
{
	od_thread_pool_worker_t *worker = arg;
	od_thread_pool_t *pool = worker->pool;

	while (1) {
		uint64_t version = atomic_load(&pool->version);

		od_thread_pool_task_t task;
		if (mm_queue_pop(&pool->queue, &task)) {
			if (task.fn == NULL) {
				break;
			}

			void *result = task.fn(task.arg);
			atomic_store(&task.future->value, (uintptr_t)result);
			mm_wait_flag_set(task.future->wait);
			od_future_unref(task.future);

			continue;
		}

		mm_wait_list_compare_wait(&pool->notifier, NULL, version, 1000);
	}
}

static void notify_one(od_thread_pool_t *pool)
{
	atomic_fetch_add(&pool->version, 1);
	mm_wait_list_notify(&pool->notifier);
}

static void stop(od_thread_pool_t *pool)
{
	uint64_t exp = 0;
	if (!atomic_compare_exchange_strong(&pool->stop, &exp, 1)) {
		return;
	}

	for (size_t i = 0; i < pool->size; ++i) {
		od_thread_pool_task_t cl;
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

int od_thread_pool_init(od_thread_pool_t *pool, const char *name, size_t size,
			size_t queue_size)
{
	memset(pool, 0, sizeof(od_thread_pool_t));

	strncpy(pool->name, name, sizeof(pool->name) - 1);

	int rc = mm_queue_init(&pool->queue, queue_size,
			       sizeof(od_thread_pool_task_t), task_destroy);
	if (rc != 0) {
		return rc;
	}

	pool->size = size;
	atomic_init(&pool->version, 0);
	atomic_init(&pool->stop, 0);
	mm_wait_list_init(&pool->notifier, &pool->version);

	pool->workers = od_malloc(size * sizeof(od_thread_pool_worker_t));
	if (pool->workers == NULL) {
		mm_wait_list_destroy(&pool->notifier);
		mm_queue_destroy(&pool->queue);
		return -1;
	}
	memset(pool->workers, 0, size * sizeof(od_thread_pool_worker_t));

	for (size_t i = 0; i < size; ++i) {
		od_thread_pool_worker_t *wrk = &pool->workers[i];

		wrk->pool = pool;
		wrk->idx = i;

		od_snprintf(wrk->name, sizeof(wrk->name), "%s_%lu", name, i);

		wrk->machine_id = machine_create(wrk->name, tp_work, wrk);
		if (wrk->machine_id == -1) {
			pool->size = i;
			od_thread_pool_destroy(pool);
			return -1;
		}
	}

	return 0;
}

void od_thread_pool_destroy(od_thread_pool_t *pool)
{
	stop(pool);

	for (size_t j = 0; j < pool->size; ++j) {
		machine_wait(pool->workers[j].machine_id);
	}

	mm_free(pool->workers);
	mm_queue_destroy(&pool->queue);
	mm_wait_list_destroy(&pool->notifier);
}

od_future_t *od_thread_pool_submit(od_thread_pool_t *pool,
				   od_thread_pool_task_fn_t fn, void *arg)
{
	od_future_t *future = od_future_create();
	if (future == NULL) {
		return NULL;
	}

	od_thread_pool_task_t task;
	memset(&task, 0, sizeof(od_thread_pool_task_t));
	task.future = future;
	task.fn = fn;
	task.arg = arg;

	/* one another ref for 'user' */
	od_future_ref(future);

	if (mm_queue_push(&pool->queue, &task)) {
		notify_one(pool);
		return future;
	}

	/* no actual 'user' exists, so double-unref */
	od_future_unref(future);
	od_future_unref(future);

	mm_errno_set(EAGAIN);
	return NULL;
}

int od_thread_pool_wait(od_future_t *future, uint32_t timeout_ms)
{
	int rc = mm_wait_flag_wait(future->wait, timeout_ms);

	return rc;
}
