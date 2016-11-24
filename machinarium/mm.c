
/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

static void
mm_async_cb(uv_async_t *handle)
{
	mm *f = handle->data;
	while (f->scheduler.count_ready > 0) {
		mmfiber *fiber =
			mm_scheduler_next_ready(&f->scheduler);
		mm_scheduler_set(fiber, MM_FACTIVE);
		mm_scheduler_call(fiber);
	}
}

static void
mm_timer_cb(uv_timer_t *handle)
{
	mmfiber *fiber = handle->data;
	mm *f = fiber->data;
	mm_wakeup(f, fiber);
}

static void
mm_close_cb(uv_handle_t *handle, void *arg)
{
	if (! uv_is_closing(handle))
		uv_close(handle, NULL);
}

MM_API mm_t
mm_new(void)
{
	mm *handle = malloc(sizeof(*handle));
	if (handle == NULL)
		return NULL;
	handle->online = 0;
	mm_scheduler_init(&handle->scheduler, 16 * 1024, handle);
	int rc = uv_loop_init(&handle->loop);
	if (rc < 0)
		return NULL;
	uv_async_init(&handle->loop, &handle->async, mm_async_cb);
	handle->async.data = handle;
	return (mm_t)handle;
}

MM_API int
mm_free(mm_t envp)
{
	mm *env = envp;
	if (env->online)
		return -1;

	/* force free event loop handles */
	uv_walk(&env->loop, mm_close_cb, NULL);
	uv_run(&env->loop, UV_RUN_DEFAULT);
	uv_stop(&env->loop);
	uv_loop_close(&env->loop);
	mm_scheduler_free(&env->scheduler);

	free(envp);
	return 0;
}

MM_API int64_t
mm_create(mm_t envp, mmfunction_t function, void *arg)
{
	mm *env = envp;
	mmfiber *fiber =
		mm_scheduler_new(&env->scheduler, (mmfiberf)function, arg);
	if (fiber == NULL)
		return -1;
	uv_timer_init(&env->loop, &fiber->timer);
	fiber->timer.data = fiber;
	fiber->data = env;
	return fiber->id;
}

MM_API int
mm_is_online(mm_t envp)
{
	mm *env = envp;
	return env->online;
}

MM_API int
mm_is_cancel(mm_t envp)
{
	mm *env = envp;
	mmfiber *fiber = mm_scheduler_current(&env->scheduler);
	if (fiber == NULL)
		return -1;
	return fiber->cancel > 0;
}

MM_API void
mm_start(mm_t envp)
{
	mm *env = envp;
	uv_async_send(&env->async);
	env->online = 1;
	for (;;) {
		/* do graceful shutdown */
		if (! env->online) {
			if (! mm_scheduler_online(&env->scheduler))
				break;
		}
		uv_run(&env->loop, UV_RUN_ONCE);
	}
}

MM_API void
mm_stop(mm_t envp)
{
	mm *env = envp;
	env->online = 0;
}

static inline void
mm_sleep_cancel_cb(mmfiber *fiber, void *arg)
{
	uv_timer_stop(&fiber->timer);
	uv_handle_t *handle = (uv_handle_t*)&fiber->timer;
	if (! uv_is_closing(handle))
		uv_close(handle, NULL);
	mm *f = fiber->data;
	mm_wakeup(f, fiber);
}

MM_API void
mm_sleep(mm_t envp, uint64_t time_ms)
{
	mm *env = envp;
	mmfiber *fiber = mm_scheduler_current(&env->scheduler);
	if (mm_fiber_is_cancel(fiber))
		return;
	uv_timer_start(&fiber->timer, mm_timer_cb, time_ms, 0);
	mm_fiber_op_begin(fiber, mm_sleep_cancel_cb, NULL);
	mm_scheduler_yield(&env->scheduler);
	mm_fiber_op_end(fiber);
}

MM_API int
mm_wait(mm_t envp, uint64_t id)
{
	mm *env = envp;
	mmfiber *fiber = mm_scheduler_match(&env->scheduler, id);
	if (fiber == NULL)
		return -1;
	mmfiber *waiter = mm_scheduler_current(&env->scheduler);
	mm_scheduler_wait(fiber, waiter);
	mm_scheduler_yield(&env->scheduler);
	return 0;
}

MM_API int
mm_cancel(mm_t envp, uint64_t id)
{
	mm *env = envp;
	mmfiber *fiber = mm_scheduler_match(&env->scheduler, id);
	if (fiber == NULL)
		return -1;
	mm_fiber_cancel(fiber);
	return 0;
}
