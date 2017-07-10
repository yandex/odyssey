#ifndef MM_TLS_IO_H
#define MM_TLS_IO_H

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_tlsio mm_tlsio_t;

struct mm_tlsio
{
	SSL_CTX *ctx;
	SSL     *ssl;
	BIO     *bio;
	int      error;
	char     error_msg[128];
	uint32_t time_ms;
	void    *io;
};

static inline int
mm_tls_is_active(mm_tlsio_t *io) {
	return io->ssl != NULL;
}

void mm_tls_init(void);
void mm_tls_free(void);
void mm_tlsio_init(mm_tlsio_t*, void*);
void mm_tlsio_free(mm_tlsio_t*);
void mm_tlsio_error_reset(mm_tlsio_t*);
int  mm_tlsio_connect(mm_tlsio_t*, mm_tls_t*);
int  mm_tlsio_accept(mm_tlsio_t*, mm_tls_t*);
int  mm_tlsio_close(mm_tlsio_t*);
int  mm_tlsio_write(mm_tlsio_t*, char*, int, uint32_t);
int  mm_tlsio_read(mm_tlsio_t*, char*, int, uint32_t);

#endif /* MM_TLS_IO_H */
