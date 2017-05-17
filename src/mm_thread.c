
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

int mm_thread_create(mm_thread_t *thread, mm_thread_function_t function, void *arg)
{
	thread->function = function;
	thread->arg = arg;
	int rc;
	rc = pthread_create(&thread->id, NULL, function, arg);
	if (rc != 0)
		return -1;
	return 0;
}

int mm_thread_join(mm_thread_t *thread)
{
	int rc;
	rc = pthread_join(thread->id, NULL);
	return rc;
}
