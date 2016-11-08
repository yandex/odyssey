#ifndef FLUENT_H_
#define FLUENT_H_

/*
 * libfluent.
 *
 * Cooperative multitasking engine.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if __GNUC__ >= 4
#  define FLUENT_API __attribute__((visibility("default")))
#else
#  define FLUENT_API
#endif

typedef void* fluent_t;

typedef void (*fluent_function_t)(void *arg);

FLUENT_API fluent_t ft_new(void);
FLUENT_API int      ft_free(fluent_t);
FLUENT_API int      ft_create(fluent_t, fluent_function_t, void *arg);
FLUENT_API int      ft_is_online(fluent_t);
FLUENT_API void     ft_start(fluent_t);
FLUENT_API void     ft_stop(fluent_t);
FLUENT_API void     ft_sleep(fluent_t, uint64_t time_ms);

#endif
