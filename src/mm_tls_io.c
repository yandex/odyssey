
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
	BIO *bio;
	SSL_CTX *ctx;
	SSL *ssl;

	SSL_METHOD *method;
	method = (SSL_METHOD*)TLS_client_method();
	if (method == NULL)
		return -1;
	ctx = SSL_CTX_new(method);
	if (ctx == NULL)
		return -1;

	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
	SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);

	/* cert file */
	int rc;
	if (tls->cert_file) {
		rc = SSL_CTX_use_certificate_chain_file(ctx, tls->cert_file);
		if (! rc) {
			return -1;
		}
	}
	/* key file */
	if (tls->key_file) {
		rc = SSL_CTX_use_PrivateKey_file(ctx, tls->key_file, SSL_FILETYPE_PEM);
		if (! rc) {
			return -1;
		}
	}
	/*
	rc = SSL_CTX_check_private_key(ctx);
	if(! rc) {
		return -1;
	}
	*/

	/* ca file and ca_path */
	if (tls->ca_file || tls->ca_path) {
		rc = SSL_CTX_load_verify_locations(ctx, tls->ca_file, tls->ca_path);
		if (! rc) {
			return -1;
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
		return -1;
	}

	BIO_METHOD *bio_method;
	bio_method = BIO_meth_new(BIO_TYPE_MEM, "machinarium-tls");
	if (bio_method == NULL) {
		return -1;
	}
	BIO_meth_set_write(bio_method, mm_tlsio_write_cb);
	BIO_meth_set_read(bio_method, mm_tlsio_read_cb);

	bio = BIO_new(bio_method);
	if (bio == NULL) {
		return -1;
	}
	BIO_set_app_data(bio, io);
	SSL_set_bio(ssl, bio, bio);

	io->ctx        = ctx;
	io->ssl        = ssl;
	io->bio        = bio;
	io->bio_method = bio_method;
	io->io         = NULL;
	return 0;
}

int mm_tlsio_connect(mm_tlsio_t *io, mm_tls_t *tls)
{
	int rc;
	rc = mm_tlsio_prepare(tls, io);
	if (rc == -1)
		return -1;
	rc = SSL_connect(io->ssl);
	if(! rc) {
		return -1;
	}
	/* verify */
	return 0;
}

int mm_tlsio_close(mm_tlsio_t *io)
{
	(void)io;
	return 0;
}

int mm_tlsio_write(mm_tlsio_t *io, char *buf, int size)
{
	int rc;
	rc = SSL_write(io->ssl, buf, size);
	if (rc <= 0)
		return -1;
	return 0;
}

int mm_tlsio_read(mm_tlsio_t *io, char *buf, int size)
{
	int rc;
	rc = SSL_read(io->ssl, buf, size);
	if (rc <= 0)
		return -1;
	return 0;
}
