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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#if __GNUC__ >= 4
#  define MM_API __attribute__((visibility("default")))
#else
#  define MM_API
#endif

typedef void (*mm_function_t)(void *arg);

typedef void* mm_t;
typedef void* mm_io_t;

MM_API mm_t    mm_new(void);
MM_API int     mm_free(mm_t);
MM_API int64_t mm_create(mm_t, mm_function_t, void *arg);
MM_API int     mm_is_online(mm_t);
MM_API int     mm_is_cancel(mm_t);
MM_API void    mm_start(mm_t);
MM_API void    mm_stop(mm_t);
MM_API void    mm_sleep(mm_t, uint64_t time_ms);
MM_API int     mm_wait(mm_t, uint64_t id);
MM_API int     mm_cancel(mm_t, uint64_t id);

MM_API mm_io_t mm_io_new(mm_t);
MM_API int     mm_io_fd(mm_io_t);
MM_API int     mm_io_nodelay(mm_io_t, int enable);
MM_API int     mm_io_keepalive(mm_io_t, int enable, int delay);
MM_API int     mm_io_readahead(mm_io_t, int size);
MM_API int     mm_connect(mm_io_t, struct sockaddr*, uint64_t time_ms);
MM_API int     mm_connect_is_timeout(mm_io_t);
MM_API int     mm_is_connected(mm_io_t);
MM_API int     mm_bind(mm_io_t, struct sockaddr*);
MM_API int     mm_accept(mm_io_t, int backlog, mm_io_t *client);
MM_API int     mm_read(mm_io_t, int size, uint64_t time_ms);
MM_API int     mm_read_is_timeout(mm_io_t);
MM_API char   *mm_read_buf(mm_io_t);
MM_API int     mm_write(mm_io_t, char *buf, int size, uint64_t time_ms);
MM_API int     mm_write_is_timeout(mm_io_t);
MM_API void    mm_close(mm_io_t);
MM_API int     mm_getsockname(mm_io_t, struct sockaddr*, int*);
MM_API int     mm_getaddrinfo(mm_io_t, char *addr, char *service,
                              struct addrinfo *hints,
                              struct addrinfo **res,
                              uint64_t time_ms);

#endif
