
/*
 * libfluent.
 *
 * Cooperative multitasking engine.
*/

#include <fluent_private.h>
#include <fluent.h>

static void
ft_async_cb(uv_async_t *handle)
{
	ft *f = handle->data;
	while (f->scheduler.count_ready > 0) {
		ftfiber *fiber =
			ft_scheduler_next_ready(&f->scheduler);
		ft_scheduler_set(fiber, FT_FACTIVE);
		ft_scheduler_call(fiber);
	}
}

static void
ft_timer_cb(uv_timer_t *handle)
{
	ftfiber *fiber = handle->data;
	ft *f = fiber->data;
	ft_wakeup(f, fiber);
}

static void
ft_close_cb(uv_handle_t *handle, void *arg)
{
	if (! uv_is_closing(handle))
		uv_close(handle, NULL);
}

FLUENT_API fluent_t
ft_new(void)
{
	ft *handle = malloc(sizeof(*handle));
	if (handle == NULL)
		return NULL;
	handle->online = 0;
	ft_scheduler_init(&handle->scheduler, 16 * 1024, handle);
	int rc = uv_loop_init(&handle->loop);
	if (rc < 0)
		return NULL;
	uv_async_init(&handle->loop, &handle->async, ft_async_cb);
	handle->async.data = handle;
	return (fluent_t)handle;
}

FLUENT_API int
ft_free(fluent_t envp)
{
	ft *env = envp;
	if (env->online)
		return -1;

	/* force free event loop handles */
	uv_walk(&env->loop, ft_close_cb, NULL);
	uv_run(&env->loop, UV_RUN_DEFAULT);
	uv_stop(&env->loop);
	uv_loop_close(&env->loop);
	ft_scheduler_free(&env->scheduler);

	free(envp);
	return 0;
}

FLUENT_API int
ft_create(fluent_t envp, fluent_function_t function, void *arg)
{
	ft *env = envp;
	ftfiber *fiber =
		ft_scheduler_new(&env->scheduler, (ftfiberf)function, arg);
	if (fiber == NULL)
		return -1;
	uv_timer_init(&env->loop, &fiber->timer);
	fiber->timer.data = fiber;
	fiber->data = env;
	return 0;
}

FLUENT_API int
ft_is_online(fluent_t envp)
{
	ft *env = envp;
	return env->online;
}

FLUENT_API void
ft_start(fluent_t envp)
{
	ft *env = envp;
	uv_async_send(&env->async);
	env->online = 1;
	for (;;) {
		/* do graceful shutdown */
		if (! env->online) {
			if (! ft_scheduler_online(&env->scheduler))
				break;
		}
		uv_run(&env->loop, UV_RUN_ONCE);
	}
}

FLUENT_API void
ft_stop(fluent_t envp)
{
	ft *env = envp;
	env->online = 0;
}

FLUENT_API void
ft_sleep(fluent_t envp, uint64_t time_ms)
{
	ft *env = envp;
	ftfiber *fiber = ft_scheduler_current(&env->scheduler);
	uv_timer_start(&fiber->timer, ft_timer_cb, time_ms, 0);
	ft_scheduler_yield(&env->scheduler);
}
