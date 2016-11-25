#ifndef MACHINARIUM_H_
#define MACHINARIUM_H_

/*
 * machinarium.
 *
 * Cooperative multitasking engine.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#if __GNUC__ >= 4
#  define MM_API __attribute__((visibility("default")))
#else
#  define MM_API
#endif

typedef void (*mmfunction_t)(void *arg);

typedef void* mm_t;
typedef void* mmio_t;

MM_API mm_t
mm_new(void);

MM_API int     mm_free(mm_t);
MM_API int64_t mm_create(mm_t, mmfunction_t, void *arg);
MM_API int     mm_is_online(mm_t);
MM_API int     mm_is_cancel(mm_t);
MM_API void    mm_start(mm_t);
MM_API void    mm_stop(mm_t);
MM_API void    mm_sleep(mm_t, uint64_t time_ms);
MM_API int     mm_wait(mm_t, uint64_t id);
MM_API int     mm_cancel(mm_t, uint64_t id);

MM_API mmio_t
mm_io_new(mm_t);

MM_API void    mm_close(mmio_t);
MM_API int     mm_fd(mmio_t);
MM_API int     mm_connect(mmio_t, char *addr, int port, uint64_t time_ms);
MM_API int     mm_connect_is_timeout(mmio_t);
MM_API int     mm_is_connected(mmio_t);
MM_API int     mm_bind(mmio_t, char *addr, int port);
MM_API int     mm_accept(mmio_t, int backlog, mmio_t *client);
MM_API int     mm_read(mmio_t, int size, uint64_t time_ms);
MM_API int     mm_read_is_timeout(mmio_t);
MM_API char   *mm_read_buf(mmio_t);
MM_API int     mm_write(mmio_t, char *buf, int size, uint64_t time_ms);
MM_API int     mm_write_is_timeout(mmio_t);

#endif
