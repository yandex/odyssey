#ifndef FLUENT_H_
#define FLUENT_H_

/*
 * fluent.
 *
 * Cooperative multitasking engine.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#if __GNUC__ >= 4
#  define FLUENT_API __attribute__((visibility("default")))
#else
#  define FLUENT_API
#endif

typedef void (*ftfunction_t)(void *arg);

typedef void* ft_t;
typedef void* ftio_t;

FLUENT_API ft_t
ft_new(void);

FLUENT_API int   ft_free(ft_t);
FLUENT_API int   ft_create(ft_t, ftfunction_t, void *arg);
FLUENT_API int   ft_is_online(ft_t);
FLUENT_API void  ft_start(ft_t);
FLUENT_API void  ft_stop(ft_t);
FLUENT_API void  ft_sleep(ft_t, uint64_t time_ms);

FLUENT_API ftio_t
ft_io_new(ft_t);

FLUENT_API void  ft_close(ftio_t);
FLUENT_API int   ft_fd(ftio_t);
FLUENT_API int   ft_is_connected(ftio_t);
FLUENT_API int   ft_connect(ftio_t, char *addr, int port, uint64_t time_ms);
FLUENT_API int   ft_connect_is_timeout(ftio_t);

#endif
