#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <openssl/ssl.h>

#include <machinarium/machinarium.h>

#include <machinarium/fd.h>
#include <machinarium/cond.h>
#include <machinarium/tls.h>

typedef struct mm_tls mm_tls_t;
typedef struct mm_tls_ctx mm_tls_ctx_t;

typedef enum { MM_TLS_NONE, MM_TLS_PEER, MM_TLS_PEER_STRICT } mm_tlsverify_t;

struct mm_tls {
	mm_tlsverify_t verify;
	char *server;
	char *protocols;
	char *ca_path;
	char *ca_file;
	char *cert_file;
	char *key_file;
};

struct mm_tls_ctx {
	mm_tls_t *key;
	SSL_CTX *tls_ctx;
	mm_tls_ctx_t *next;
};

struct mm_io {
	int fd;
	mm_fd_t handle;
	int attached;
	int is_unix_socket;
	int is_eventfd;
	int opt_nodelay;
	int opt_cloexec;
	/* tcp keepalive */
	int opt_keepalive;
	int opt_keepalive_delay;
	int opt_keepalive_interval;
	int opt_keepalive_probes;
	int opt_keepalive_usr_timeout;
	/* tls */
	mm_tls_t *tls;
	SSL *tls_ssl;
	int tls_error;
	char tls_error_msg[128];
	/* connect */
	int connected;
	/* accept */
	int accepted;
	int accept_listen;
	/* io */
	int last_event;
	mm_cond_t cond;
	int errored;
	int error;
	mm_call_t call;
	/* compression */
	mm_zpq_stream_t *zpq_stream;

	uint64_t deadline_ms;
};

mm_io_t *mm_io_create(void);
void mm_io_free(mm_io_t *);

int mm_io_socket_set(mm_io_t *, int);
int mm_io_socket(mm_io_t *, struct sockaddr *);
ssize_t mm_io_write(mm_io_t *, const void *, size_t);
ssize_t mm_io_read(mm_io_t *, void *, size_t);
int mm_io_format_socket_addr(mm_io_t *, char *, size_t);
int mm_io_read_pending(mm_io_t *);

void mm_io_set_peer(mm_io_t *io, mm_io_t *peer);
void mm_io_remove_peer(mm_io_t *io, mm_io_t *peer);
int mm_io_wait(mm_io_t *io, uint32_t timeout_ms);

int mm_io_verify(mm_io_t *, char *common_name);
int mm_io_format_socket_addr(mm_io_t *io, char *buf, size_t buflen);

int mm_io_attach(mm_io_t *);
int mm_io_detach(mm_io_t *);
int mm_io_is_tls(mm_io_t *);
char *mm_io_error(mm_io_t *);
int mm_io_fd(mm_io_t *);
int mm_io_set_nodelay(mm_io_t *, int enable);
int mm_io_set_keepalive(mm_io_t *, int enable, int delay, int interval,
			int probes, int usr_timeout);
int mm_io_set_nolinger(mm_io_t *io);
int mm_io_advice_keepalive_usr_timeout(int delay, int interval, int probes);
int mm_io_set_tls(mm_io_t *, machine_tls_t *, uint32_t);
int mm_io_set_compression(mm_io_t *, char algorithm);
int mm_io_connect(mm_io_t *, struct sockaddr *, uint32_t time_ms);
int mm_io_connected(mm_io_t *);
int mm_io_bind(mm_io_t *, struct sockaddr *, int);
int mm_io_accept(mm_io_t *, mm_io_t **, int backlog, int attach,
		 uint32_t time_ms);
int mm_io_close(mm_io_t *);
int mm_io_shutdown(mm_io_t *);
int mm_io_shutdown_receptions(mm_io_t *);
int mm_io_set_tls(mm_io_t *obj, machine_tls_t *tls, uint32_t timeout);

int mm_io_last_event(mm_io_t *io);

/* now + timeout_ms */
void mm_io_set_deadline(mm_io_t *io, uint32_t timeout_ms);
int mm_io_wait_deadline(mm_io_t *io);
