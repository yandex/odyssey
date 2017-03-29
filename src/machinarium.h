#ifndef MACHINARIUM_H_
#define MACHINARIUM_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#if __GNUC__ >= 4
#  define MACHINE_API __attribute__((visibility("default")))
#else
#  define MACHINE_API
#endif

typedef void (*machine_fiber_function_t)(void *arg);

typedef void* machine_t;
typedef void* machine_tls_t;
typedef void* machine_io_t;

/* library control */

MACHINE_API int
machinarium_init(void);

MACHINE_API void
machinarium_free(void);

/* machine control */

MACHINE_API machine_t
machine_create(void);

MACHINE_API int
machine_free(machine_t);

MACHINE_API void
machine_start(machine_t);

MACHINE_API void
machine_stop(machine_t);

MACHINE_API int
machine_active(machine_t);

/* fiber */

MACHINE_API int64_t
machine_create_fiber(machine_t, machine_fiber_function_t, void *arg);

MACHINE_API void
machine_sleep(machine_t, uint64_t time_ms);

MACHINE_API int
machine_wait(machine_t, uint64_t);

MACHINE_API int
machine_cancel(machine_t, uint64_t);

MACHINE_API int
machine_condition(machine_t, uint64_t time_ms);

MACHINE_API int
machine_signal(machine_t, uint64_t);

MACHINE_API int
machine_cancelled(machine_t);

/* tls */

MACHINE_API machine_tls_t
machine_create_tls(machine_t);

MACHINE_API void
machine_free_tls(machine_tls_t);

MACHINE_API int
machine_tls_set_mode(machine_tls_t, char*);

MACHINE_API int
machine_tls_set_protocols(machine_tls_t, char*);

MACHINE_API int
machine_tls_set_ca_path(machine_tls_t, char*);

MACHINE_API int
machine_tls_set_ca_file(machine_tls_t, char*);

MACHINE_API int
machine_tls_set_cert_file(machine_tls_t, char*);

MACHINE_API int
machine_tls_set_key_file(machine_tls_t, char*);

/* io control */

MACHINE_API machine_io_t
machine_create_io(machine_t);

MACHINE_API void
machine_close(machine_io_t);

MACHINE_API int
machine_errno(machine_io_t);

MACHINE_API char*
machine_error(machine_io_t);

MACHINE_API int
machine_fd(machine_io_t);

MACHINE_API void
machine_set_tls(machine_io_t, machine_tls_t);

MACHINE_API int
machine_set_nodelay(machine_io_t, int enable);

MACHINE_API int
machine_set_keepalive(machine_io_t, int enable, int delay);

MACHINE_API int
machine_set_readahead(machine_io_t, int size);

/* dns */

MACHINE_API int
machine_getsockname(machine_io_t, struct sockaddr*, int*);

MACHINE_API int
machine_getpeername(machine_io_t, struct sockaddr*, int*);

MACHINE_API int
machine_getaddrinfo(machine_io_t, char *addr, char *service,
                    struct addrinfo *hints,
                    struct addrinfo **res,
                    uint64_t time_ms);

/* io */

MACHINE_API int
machine_connect(machine_io_t, struct sockaddr*, uint64_t time_ms);

MACHINE_API int
machine_connect_timedout(machine_io_t);

MACHINE_API int
machine_connected(machine_io_t);

MACHINE_API int
machine_bind(machine_io_t, struct sockaddr*);

MACHINE_API int
machine_accept(machine_io_t, int backlog, machine_io_t *client);

MACHINE_API int
machine_read(machine_io_t, char *buf, int size, uint64_t time_ms);

MACHINE_API int
machine_read_timedout(machine_io_t);

MACHINE_API char*
machine_read_buf(machine_io_t);

MACHINE_API int
machine_write(machine_io_t, char *buf, int size, uint64_t time_ms);

MACHINE_API int
machine_write_timedout(machine_io_t);

#endif
