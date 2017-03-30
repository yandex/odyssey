
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
	io->error_msg[0] = 0;
	ERR_clear_error();
}

static inline void
mm_tlsio_error(mm_tlsio_t *io, int ssl_rc, char *fmt, ...)
{
	unsigned int error;
	io->error = 1;

	va_list args;
	va_start(args, fmt);
	int len = 0;
	len = vsnprintf(io->error_msg, sizeof(io->error_msg), fmt, args);
	va_end(args);

	(void)ssl_rc;
#if 0
	error = SSL_get_error(io->ssl, ssl_rc);
	switch (error) {
	case SSL_ERROR_SYSCALL:
	case SSL_ERROR_SSL:
		goto set_error_message;
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
	return;
#endif
	error = ERR_peek_last_error();
	if (error != 0) {
		len += snprintf(io->error_msg + len, sizeof(io->error_msg) - len,
		                ": %s",
		                ERR_error_string(error, NULL));
	}
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

static long
mm_tlsio_ctrl_cb(BIO *bio, int cmd, long larg, void *parg)
{
	(void)parg;
	long ret = 1;
	switch (cmd) {
	case BIO_CTRL_GET_CLOSE:
		ret = (long)BIO_get_shutdown(bio);
		break;
	case BIO_CTRL_SET_CLOSE:
		BIO_set_shutdown(bio, (int)larg);
		break;
	case BIO_CTRL_DUP:
	case BIO_CTRL_FLUSH:
		break;
	case BIO_CTRL_INFO:
	case BIO_CTRL_GET:
	case BIO_CTRL_SET:
	case BIO_CTRL_PUSH:
	case BIO_CTRL_POP:
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int
mm_tlsio_prepare(mm_tls_t *tls, mm_tlsio_t *io)
{
	SSL_CTX *ctx = NULL;
	SSL *ssl = NULL;
	BIO_METHOD *bio_method = NULL;
	BIO *bio = NULL;

	ctx = SSL_CTX_new((SSL_METHOD*)TLS_client_method());
	if (ctx == NULL) {
		mm_tlsio_error(io, 0, "SSL_CTX_new()");
		return -1;
	}

	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
	SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);

	/* verify mode */
	int verify;
	switch (tls->verify) {
	case MM_TLS_NONE:
		verify = SSL_VERIFY_NONE;
		break;
	case MM_TLS_PEER:
		verify = SSL_VERIFY_PEER;
		break;
	case MM_TLS_PEER_STRICT:
		verify = SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
		break;
	}
	SSL_CTX_set_verify(ctx, verify, NULL);
	SSL_CTX_set_verify_depth(ctx, 6);

	/* cert file */
	int rc;
	if (tls->cert_file) {
		rc = SSL_CTX_use_certificate_chain_file(ctx, tls->cert_file);
		if (! rc) {
			mm_tlsio_error(io, 0, "SSL_CTX_use_certificate_chain_file()");
			goto error;
		}
	}
	/* key file */
	if (tls->key_file) {
		rc = SSL_CTX_use_PrivateKey_file(ctx, tls->key_file, SSL_FILETYPE_PEM);
		if (! rc) {
			mm_tlsio_error(io, 0, "SSL_CTX_use_PrivateKey_file()");
			goto error;
		}
	}
	if (tls->cert_file && tls->key_file) {
		rc = SSL_CTX_check_private_key(ctx);
		if(! rc) {
			mm_tlsio_error(io, 0, "SSL_CTX_check_private_key()");
			goto error;
		}
	}

	/* ca file and ca_path */
	if (tls->ca_file || tls->ca_path) {
		rc = SSL_CTX_load_verify_locations(ctx, tls->ca_file, tls->ca_path);
		if (! rc) {
			mm_tlsio_error(io, 0, "SSL_CTX_load_verify_locations()");
			goto error;
		}
	}

	/* ocsp */
	/*
	SSL_set_cipher_list()
	SSL_set_tlsext_host_name()
	*/

	ssl = SSL_new(ctx);
	if (ssl == NULL) {
		mm_tlsio_error(io, 0, "SSL_new()");
		goto error;
	}

	bio_method = BIO_meth_new(BIO_TYPE_MEM, "machinarium-tls");
	if (bio_method == NULL) {
		mm_tlsio_error(io, 0, "SSL_meth_new()");
		goto error;
	}
	BIO_meth_set_write(bio_method, mm_tlsio_write_cb);
	BIO_meth_set_read(bio_method, mm_tlsio_read_cb);
	BIO_meth_set_ctrl(bio_method, mm_tlsio_ctrl_cb);

	bio = BIO_new(bio_method);
	if (bio == NULL) {
		mm_tlsio_error(io, 0, "BIO_new()");
		goto error;
	}
	BIO_set_app_data(bio, io);
	BIO_set_init(bio, 1);

	SSL_set_bio(ssl, bio, bio);

	io->ctx        = ctx;
	io->ssl        = ssl;
	io->bio        = bio;
	io->bio_method = bio_method;
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
	if (! rc) {
		mm_tlsio_error(io, rc, "SSL_connect()");
		return -1;
	}

	/* todo: verify */
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
		mm_tlsio_error(io, rc, "SSL_write()");
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
		mm_tlsio_error(io, rc, "SSL_read()");
		return -1;
	}
	return 0;
}
