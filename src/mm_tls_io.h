#ifndef MM_TLS_IO_H_
#define MM_TLS_IO_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_tlsio_t mm_tlsio_t;

struct mm_tlsio_t {
	SSL_CTX    *ctx;
	SSL        *ssl;
	BIO_METHOD *bio_method;
	BIO        *bio;
	int         error;
	char        error_msg[128];
	void       *io;
};

static inline int
mm_tls_is_active(mm_tlsio_t *io) {
	return io->ssl != NULL;
}

void mm_tls_init(void);
void mm_tls_free(void);
void mm_tlsio_init(mm_tlsio_t*, void*);
void mm_tlsio_free(mm_tlsio_t*);
int  mm_tlsio_connect(mm_tlsio_t*, mm_tls_t*);
int  mm_tlsio_accept(mm_tlsio_t*, mm_tls_t*);
int  mm_tlsio_close(mm_tlsio_t*);
int  mm_tlsio_write(mm_tlsio_t*, char*, int);
int  mm_tlsio_read(mm_tlsio_t*, char*, int);

#endif
