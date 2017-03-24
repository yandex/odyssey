#ifndef MM_TLS_IO_H_
#define MM_TLS_IO_H_

/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

typedef struct mm_tlsio_t mm_tlsio_t;

struct mm_tlsio_t {
	SSL_CTX *ctx;
	SSL     *ssl;
	BIO     *io;
};

int mm_tls_openssl_init(void);

#endif
