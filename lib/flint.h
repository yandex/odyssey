#ifndef FLINT_H_
#define FLINT_H_

/*
 * flint.
 *
 * Cooperative multitasking engine.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#if __GNUC__ >= 4
#  define FLINT_API __attribute__((visibility("default")))
#else
#  define FLINT_API
#endif

typedef void (*ftfunction_t)(void *arg);

typedef void* ft_t;
typedef void* ftio_t;

FLINT_API ft_t
ft_new(void);

FLINT_API int   ft_free(ft_t);
FLINT_API int   ft_create(ft_t, ftfunction_t, void *arg);
FLINT_API int   ft_is_online(ft_t);
FLINT_API void  ft_start(ft_t);
FLINT_API void  ft_stop(ft_t);
FLINT_API void  ft_sleep(ft_t, uint64_t time_ms);

FLINT_API ftio_t
ft_io_new(ft_t);

FLINT_API void  ft_close(ftio_t);
FLINT_API int   ft_fd(ftio_t);
FLINT_API int   ft_is_connected(ftio_t);
FLINT_API int   ft_connect(ftio_t, char *addr, int port, uint64_t time_ms);
FLINT_API int   ft_connect_is_timeout(ftio_t);
FLINT_API int   ft_bind(ftio_t, char *addr, int port);
FLINT_API int   ft_accept(ftio_t, ftio_t *client);
FLINT_API int   ft_read(ftio_t, int size, uint64_t time_ms);
FLINT_API int   ft_read_is_timeout(ftio_t);
FLINT_API char *ft_read_buf(ftio_t);
FLINT_API int   ft_write(ftio_t, char *buf, int size, uint64_t time_ms);
FLINT_API int   ft_write_is_timeout(ftio_t);

#endif
