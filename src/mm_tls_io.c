
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

void mm_tls_init(void)
{
	SSL_library_init();
	SSL_load_error_strings();
}

void mm_tlsio_init(mm_tlsio_t *io, void *io_arg)
{
	memset(io, 0, sizeof(*io));
	io->io = io_arg;
}

void mm_tlsio_free(mm_tlsio_t *io)
{
	(void)io;
}

static inline void
mm_tlsio_error_reset(mm_tlsio_t *io)
{
	io->error = 0;
	io->error_code = 0;
}

static inline void
mm_tlsio_error(mm_tlsio_t *io)
{
	io->error = 2;
}

static inline void
mm_tlsio_error_ssl(mm_tlsio_t *io, int rc)
{
	io->error = 1;
	io->error_code = SSL_get_error(io->ssl, rc);
	if (io->error_code == SSL_ERROR_SYSCALL)
		mm_io_set_errno(io->io, errno);
}

char *mm_tlsio_strerror(mm_tlsio_t *io)
{
	unsigned long error;
	if (io->error == 0)
		return NULL;
	if (io->error == 1) {
		error = ERR_get_error();
		if (error != 0)
			return ERR_error_string(error, NULL);
		return "unknown ssl error";
	}
	assert(io->error == 2);
	switch (io->error_code) {
	case SSL_ERROR_SSL:
		error = ERR_get_error();
		if (error != 0)
			return ERR_error_string(error, NULL);
		return "unknown ssl error";
	case SSL_ERROR_SYSCALL:
		error = ERR_get_error();
		if (error != 0)
			return ERR_error_string(error, NULL);
		/* use errno */
		return NULL;
	case SSL_ERROR_NONE:
	case SSL_ERROR_ZERO_RETURN:
	case SSL_ERROR_WANT_CONNECT:
	case SSL_ERROR_WANT_ACCEPT:
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_X509_LOOKUP:
	default:
		break;
	}
	return "unhandled ssl error";
}

static int
mm_tlsio_write_cb(BIO *bio, const char *buf, int size)
{
	mm_tlsio_t *io;
	io = BIO_get_app_data(bio);
	int rc = mm_write(io->io, (char*)buf, size, 0);
	if (rc == -1)
		return -1;
	return size;
}

static int
mm_tlsio_read_cb(BIO *bio, char *buf, int size)
{
	mm_tlsio_t *io;
	io = BIO_get_app_data(bio);
	int rc = mm_read(io->io, buf, size, 0);
	if (rc == -1)
		return -1;
	return size;
}

static int
mm_tlsio_prepare(mm_tls_t *tls, mm_tlsio_t *io)
{
	SSL_CTX *ctx = NULL;
	SSL_METHOD *method = NULL;
	SSL *ssl = NULL;
	BIO_METHOD *bio_method = NULL;
	BIO *bio = NULL;

	method = (SSL_METHOD*)TLS_client_method();
	if (method == NULL) {
		mm_tlsio_error(io);
		return -1;
	}
	ctx = SSL_CTX_new(method);
	if (ctx == NULL) {
		mm_tlsio_error(io);
		return -1;
	}

	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
	SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);

	/* cert file */
	int rc;
	if (tls->cert_file) {
		rc = SSL_CTX_use_certificate_chain_file(ctx, tls->cert_file);
		if (! rc) {
			mm_tlsio_error(io);
			goto error;
		}
	}
	/* key file */
	if (tls->key_file) {
		rc = SSL_CTX_use_PrivateKey_file(ctx, tls->key_file, SSL_FILETYPE_PEM);
		if (! rc) {
			mm_tlsio_error(io);
			goto error;
		}
	}
	rc = SSL_CTX_check_private_key(ctx);
	if(! rc) {
		mm_tlsio_error(io);
		goto error;
	}

	/* ca file and ca_path */
	if (tls->ca_file || tls->ca_path) {
		rc = SSL_CTX_load_verify_locations(ctx, tls->ca_file, tls->ca_path);
		if (! rc) {
			mm_tlsio_error(io);
			goto error;
		}
	}

	/* verify mode */
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	SSL_CTX_set_verify_depth(ctx, 6);

	/* ocsp */
	/*
	SSL_set_cipher_list()
	SSL_set_tlsext_host_name()
	*/

	ssl = SSL_new(ctx);
	if (ssl == NULL) {
		mm_tlsio_error(io);
		goto error;
	}

	bio_method = BIO_meth_new(BIO_TYPE_MEM, "machinarium-tls");
	if (bio_method == NULL) {
		mm_tlsio_error(io);
		goto error;
	}
	BIO_meth_set_write(bio_method, mm_tlsio_write_cb);
	BIO_meth_set_read(bio_method, mm_tlsio_read_cb);

	bio = BIO_new(bio_method);
	if (bio == NULL) {
		mm_tlsio_error(io);
		goto error;
	}
	BIO_set_app_data(bio, io);
	SSL_set_bio(ssl, bio, bio);

	io->ctx        = ctx;
	io->ssl        = ssl;
	io->bio        = bio;
	io->bio_method = bio_method;
	io->io         = NULL;
	return 0;
error:
	SSL_CTX_free(ctx);
	if (ssl)
		SSL_free(ssl);
	if (bio)
		BIO_free(bio);
	if (bio_method)
		BIO_meth_free(bio_method);
	return -1;
}

int mm_tlsio_connect(mm_tlsio_t *io, mm_tls_t *tls)
{
	mm_tlsio_error_reset(io);
	int rc;
	rc = mm_tlsio_prepare(tls, io);
	if (rc == -1)
		return -1;
	rc = SSL_connect(io->ssl);
	if(! rc) {
		mm_tlsio_error_ssl(io, rc);
		return -1;
	}
	/* verify */
	return 0;
}

int mm_tlsio_close(mm_tlsio_t *io)
{
	mm_tlsio_error_reset(io);
	(void)io;
	return 0;
}

int mm_tlsio_write(mm_tlsio_t *io, char *buf, int size)
{
	mm_tlsio_error_reset(io);
	int rc;
	rc = SSL_write(io->ssl, buf, size);
	if (rc <= 0) {
		mm_tlsio_error_ssl(io, rc);
		return -1;
	}
	return 0;
}

int mm_tlsio_read(mm_tlsio_t *io, char *buf, int size)
{
	mm_tlsio_error_reset(io);
	int rc;
	rc = SSL_read(io->ssl, buf, size);
	if (rc <= 0) {
		mm_tlsio_error_ssl(io, rc);
		return -1;
	}
	return 0;
}
