
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium_private.h>
#include <machinarium.h>

int mm_tls_openssl_init(void)
{
	SSL_library_init();
	SSL_load_error_strings();
	return 0;
}

static int
mm_tls_write(BIO *bio, const char *buf, int size)
{
	mm_io_t *io;
	io = BIO_get_app_data(bio);
	(void)io;

	(void)bio;
	(void)buf;
	(void)size;
	return 0;
}

static int
mm_tls_read(BIO *bio, char *buf, int size)
{
	mm_io_t *io;
	io = BIO_get_app_data(bio);
	(void)io;

	(void)buf;
	(void)size;
	return 0;
}

static int
mm_tls_prepare(mm_tls_t *tls, mm_tlsio_t *io)
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
	rc = SSL_CTX_check_private_key(ctx);
	if(! rc) {
		return -1;
	}

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
	BIO_meth_set_write(bio_method, mm_tls_write);
	BIO_meth_set_read(bio_method, mm_tls_read);

	bio = BIO_new(bio_method);
	if (bio == NULL) {
		return -1;
	}
	BIO_set_app_data(bio, io);

	SSL_set_bio(ssl, bio, bio);

	/*
	BIO_do_connect()
	BIO_do_handshake()
	cert verify
	*/

	(void)ssl;
	(void)bio;
	(void)tls;
	(void)io;
	return 0;
}

int mm_tls_client(mm_tls_t *tls, mm_tlsio_t *io)
{
	int rc;
	rc = mm_tls_prepare(tls, io);
	if (rc == -1)
		return -1;
	return 0;
}
