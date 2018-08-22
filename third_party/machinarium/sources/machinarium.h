#ifndef MACHINARIUM_H
#define MACHINARIUM_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#if __GNUC__ >= 4
#  define MACHINE_API __attribute__((visibility("default")))
#else
#  define MACHINE_API
#endif

typedef void (*machine_coroutine_t)(void *arg);

/* library handles */

typedef struct machine_msg_private     machine_msg_t;
typedef struct machine_channel_private machine_channel_t;
typedef struct machine_tls_private     machine_tls_t;
typedef struct machine_io_private      machine_io_t;

/* configuration */

MACHINE_API void
machinarium_set_stack_size(int size);

MACHINE_API void
machinarium_set_pool_size(int size);

MACHINE_API void
machinarium_set_coroutine_cache_size(int size);

/* main */

MACHINE_API int
machinarium_init(void);

MACHINE_API void
machinarium_free(void);

MACHINE_API void
machinarium_stat(int *count_machine, int *count_coroutine,
                 int *count_coroutine_cache);

/* machine control */

MACHINE_API int64_t
machine_create(char *name, machine_coroutine_t, void *arg);

MACHINE_API void
machine_stop(void);

MACHINE_API int
machine_active(void);

MACHINE_API uint64_t
machine_self(void);

MACHINE_API int
machine_wait(uint64_t machine_id);

MACHINE_API uint64_t
machine_time(void);

/* signals */

MACHINE_API int
machine_signal_init(sigset_t*);

MACHINE_API int
machine_signal_wait(uint32_t time_ms);

/* coroutine */

MACHINE_API int64_t
machine_coroutine_create(machine_coroutine_t, void *arg);

MACHINE_API void
machine_sleep(uint32_t time_ms);

MACHINE_API int
machine_join(uint64_t coroutine_id);

MACHINE_API int
machine_cancel(uint64_t coroutine_id);

MACHINE_API int
machine_cancelled(void);

MACHINE_API int
machine_timedout(void);

MACHINE_API int
machine_errno(void);

MACHINE_API int
machine_condition(uint32_t time_ms);

MACHINE_API int
machine_signal(uint64_t coroutine_id);

/* msg */

MACHINE_API machine_msg_t*
machine_msg_create(int type, int data_size);

MACHINE_API void
machine_msg_free(machine_msg_t*);

MACHINE_API void*
machine_msg_get_data(machine_msg_t*);

MACHINE_API int
machine_msg_get_type(machine_msg_t*);

MACHINE_API int
machine_msg_ensure(machine_msg_t*, int size);

MACHINE_API void
machine_msg_write(machine_msg_t*, char *buf, int size);

/* channel */

MACHINE_API machine_channel_t*
machine_channel_create(int shared);

MACHINE_API void
machine_channel_free(machine_channel_t*);

MACHINE_API void
machine_channel_write(machine_channel_t*, machine_msg_t*);

MACHINE_API machine_msg_t*
machine_channel_read(machine_channel_t*, uint32_t time_ms);

/* tls */

MACHINE_API machine_tls_t*
machine_tls_create(void);

MACHINE_API void
machine_tls_free(machine_tls_t*);

MACHINE_API int
machine_tls_set_verify(machine_tls_t*, char*);

MACHINE_API int
machine_tls_set_server(machine_tls_t*, char*);

MACHINE_API int
machine_tls_set_protocols(machine_tls_t*, char*);

MACHINE_API int
machine_tls_set_ca_path(machine_tls_t*, char*);

MACHINE_API int
machine_tls_set_ca_file(machine_tls_t*, char*);

MACHINE_API int
machine_tls_set_cert_file(machine_tls_t*, char*);

MACHINE_API int
machine_tls_set_key_file(machine_tls_t*, char*);

/* io control */

MACHINE_API machine_io_t*
machine_io_create(void);

MACHINE_API void
machine_io_free(machine_io_t*);

MACHINE_API int
machine_io_attach(machine_io_t*);

MACHINE_API int
machine_io_detach(machine_io_t*);

MACHINE_API char*
machine_error(machine_io_t*);

MACHINE_API int
machine_fd(machine_io_t*);

MACHINE_API int
machine_set_nodelay(machine_io_t*, int enable);

MACHINE_API int
machine_set_keepalive(machine_io_t*, int enable, int delay);

MACHINE_API int
machine_set_readahead(machine_io_t*, int size);

MACHINE_API int
machine_set_tls(machine_io_t*, machine_tls_t*);

MACHINE_API int
machine_io_verify(machine_io_t*, char *common_name);

/* dns */

MACHINE_API int
machine_getsockname(machine_io_t*, struct sockaddr*, int*);

MACHINE_API int
machine_getpeername(machine_io_t*, struct sockaddr*, int*);

MACHINE_API int
machine_getaddrinfo(char *addr, char *service,
                    struct addrinfo *hints,
                    struct addrinfo **res,
                    uint32_t time_ms);

/* io */

MACHINE_API int
machine_connect(machine_io_t*, struct sockaddr*, uint32_t time_ms);

MACHINE_API int
machine_connected(machine_io_t*);

MACHINE_API int
machine_bind(machine_io_t*, struct sockaddr*);

MACHINE_API int
machine_accept(machine_io_t*, machine_io_t**, int backlog, int attach, uint32_t time_ms);

MACHINE_API int
machine_eventfd(machine_io_t*);

MACHINE_API int
machine_read_poll(machine_io_t**, machine_io_t**, int count, uint32_t time_ms);

MACHINE_API int
machine_read_pending(machine_io_t*);

MACHINE_API int
machine_read(machine_io_t*, char *buf, int size, uint32_t time_ms);

MACHINE_API int
machine_write(machine_io_t*, char *buf, int size, uint32_t time_ms);

MACHINE_API int
machine_close(machine_io_t*);

#ifdef __cplusplus
}
#endif

#endif /* MACHINARIUM_H */
