#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 *
 * thread_pool.h: yet another thread pool for blocking tasks execution
 */

#include <stddef.h>
#include <stdatomic.h>

#include <machinarium/wait_list.h>
#include <machinarium/wait_flag.h>
#include <machinarium/ds/queue.h>

typedef struct mm_thread_pool mm_thread_pool_t;

typedef void (*mm_thread_pool_result_dtor_t)(void *result);

typedef struct {
	atomic_uintptr_t value;
	mm_thread_pool_result_dtor_t value_dtor;
	atomic_uint_fast64_t refs;
	mm_wait_flag_t *wait;
} mm_future_t;

mm_future_t *mm_future_create(mm_thread_pool_result_dtor_t dtor);
void mm_future_ref(mm_future_t *future);
void mm_future_unref(mm_future_t *future);
void *mm_future_get_result(mm_future_t *future);

typedef void *(*mm_thread_pool_task_fn_t)(void *arg);
typedef void (*mm_thread_pool_task_arg_dtor_t)(void *arg);

typedef struct {
	mm_thread_pool_task_fn_t fn;
	void *arg;
	mm_thread_pool_task_arg_dtor_t arg_dtor;
	mm_future_t *future;
	int detached;
} mm_thread_pool_task_t;

typedef struct {
	char name[64];
	int64_t machine_id;
	size_t idx;
	mm_thread_pool_t *pool;
} mm_thread_pool_worker_t;

struct mm_thread_pool {
	char name[64];
	size_t size;
	mm_thread_pool_worker_t *workers;
	mm_queue_t queue;
	mm_wait_list_t notifier;
	atomic_uint_fast64_t version;
	atomic_uint_fast64_t stop;
};

int mm_thread_pool_init(mm_thread_pool_t *pool, const char *name, size_t size,
			size_t queue_size);
void mm_thread_pool_destroy(mm_thread_pool_t *pool);
mm_future_t *mm_thread_pool_submit(mm_thread_pool_t *pool,
				   mm_thread_pool_task_fn_t fn, void *arg,
				   mm_thread_pool_task_arg_dtor_t arg_dtor,
				   mm_thread_pool_result_dtor_t result_dtor,
				   int detach);
int mm_thread_pool_wait(mm_future_t *future, uint32_t timeout_ms);
