#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 *
 * thread_pool.h: yet another thread pool for blocking tasks execution
 */

#include <stddef.h>
#include <stdatomic.h>

#include <machinarium/wait_list.h>
#include <machinarium/wait_flag.h>
#include <machinarium/ds/queue.h>

#include <types.h>

typedef void (*od_thread_pool_result_dtor_t)(void *result);

typedef struct {
	atomic_uintptr_t value;
	od_thread_pool_result_dtor_t value_dtor;
	atomic_uint_fast64_t refs;
	mm_wait_flag_t *wait;
} od_future_t;

od_future_t *od_future_create(od_thread_pool_result_dtor_t dtor);
void od_future_ref(od_future_t *future);
void od_future_unref(od_future_t *future);
void *od_future_get_result(od_future_t *future);

typedef void *(*od_thread_pool_task_fn_t)(void *arg);
typedef void (*od_thread_pool_task_arg_dtor_t)(void *arg);

typedef struct {
	od_thread_pool_task_fn_t fn;
	void *arg;
	od_thread_pool_task_arg_dtor_t arg_dtor;
	od_future_t *future;
	int detached;
} od_thread_pool_task_t;

typedef struct {
	char name[64];
	int64_t machine_id;
	size_t idx;
	od_thread_pool_t *pool;
} od_thread_pool_worker_t;

struct od_thread_pool {
	char name[64];
	size_t size;
	od_thread_pool_worker_t *workers;
	mm_queue_t queue;
	mm_wait_list_t notifier;
	atomic_uint_fast64_t version;
	atomic_uint_fast64_t stop;
};

int od_thread_pool_init(od_thread_pool_t *pool, const char *name, size_t size,
			size_t queue_size);
void od_thread_pool_destroy(od_thread_pool_t *pool);
od_future_t *od_thread_pool_submit(od_thread_pool_t *pool,
				   od_thread_pool_task_fn_t fn, void *arg,
				   od_thread_pool_task_arg_dtor_t arg_dtor,
				   od_thread_pool_result_dtor_t result_dtor,
				   int detach);
int od_thread_pool_wait(od_future_t *future, uint32_t timeout_ms);
