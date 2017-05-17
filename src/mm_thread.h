#ifndef MM_THREAD_POOL_H_
#define MM_THREAD_POOL_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_thread_t mm_thread_t;

typedef void *(*mm_thread_function_t)(void*);

struct mm_thread_t {
	pthread_t id;
	mm_thread_function_t function;
	void *arg;
};

int mm_thread_create(mm_thread_t*, mm_thread_function_t, void*);
int mm_thread_join(mm_thread_t*);

#endif
