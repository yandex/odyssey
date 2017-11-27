
/*
 * machinarium.
 *
 * cooperative multitasking engine.
*/

#include <machinarium.h>
#include <machinarium_private.h>

void mm_tlsio_init(mm_tlsio_t *io, void *io_arg)
{
	memset(io, 0, sizeof(*io));
	io->io = io_arg;
	io->time_ms = UINT32_MAX;
}

void mm_tlsio_free(mm_tlsio_t *io)
{
	if (io->ctx)
		SSL_CTX_free(io->ctx);
	/* free io->ssl and io->bio */
	if (io->ssl)
		SSL_free(io->ssl);
}

void mm_tlsio_error_reset(mm_tlsio_t *io)
{
	mm_errno_set(0);
	io->time_ms = UINT32_MAX;
	io->error = 0;
	io->error_msg[0] = 0;
	ERR_clear_error();
}

static inline void
mm_tlsio_error(mm_tlsio_t *io, int ssl_rc, char *fmt, ...)
{
	unsigned int error;
	unsigned int error_peek;
	char *error_str;
	error_str = "unknown error";

	va_list args;
	va_start(args, fmt);
	int len = 0;
	len = mm_vsnprintf(io->error_msg, sizeof(io->error_msg), fmt, args);
	va_end(args);

	error = SSL_get_error(io->ssl, ssl_rc);
	switch (error) {
	case SSL_ERROR_NONE:
	case SSL_ERROR_ZERO_RETURN:
		break;

	case SSL_ERROR_SYSCALL:
		error_peek = ERR_peek_error();
		if (error_peek != 0) {
			error_str = ERR_error_string(error_peek, NULL);
		} else
		if (ssl_rc <= 0) {
			error_str = strerror(errno);
		}
		len += mm_snprintf(io->error_msg + len, sizeof(io->error_msg) - len,
		                   ": %s", error_str);
		break;

	case SSL_ERROR_SSL:
		error_peek = ERR_peek_error();
		if (error_peek != 0)
			error_str = ERR_error_string(error_peek, NULL);
		len += mm_snprintf(io->error_msg + len, sizeof(io->error_msg) - len,
		                   ": %s", error_str);
		break;

	case SSL_ERROR_WANT_CONNECT:
	case SSL_ERROR_WANT_ACCEPT:
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_X509_LOOKUP:
	default:
		len += mm_snprintf(io->error_msg + len, sizeof(io->error_msg) - len,
		                   ": SSL_get_error(): %d", ssl_rc);
		break;
	}

	io->error = 1;
}

static int
mm_tlsio_write_cb(BIO *bio, const char *buf, int size)
{
	mm_tlsio_t *io;
	io = BIO_get_app_data(bio);
	int rc = mm_write(io->io, (char*)buf, size, io->time_ms);
	if (rc == -1)
		return -1;
	return size;
}

static int
mm_tlsio_read_cb(BIO *bio, char *buf, int size)
{
	mm_tlsio_t *io;
	io = BIO_get_app_data(bio);
	int rc = mm_read(io->io, buf, size, io->time_ms);
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
		ret = bio->shutdown;
		break;
	case BIO_CTRL_SET_CLOSE:
		bio->shutdown = larg;
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

static BIO_METHOD mm_tlsio_method =
{
	.type   = BIO_TYPE_MEM,
	.name   = "machinarium",
	.bwrite = mm_tlsio_write_cb,
	.bread  = mm_tlsio_read_cb,
	.ctrl   = mm_tlsio_ctrl_cb
};

static int
mm_tlsio_prepare(mm_tls_t *tls, mm_tlsio_t *io, int client)
{
	SSL_CTX *ctx = NULL;
	SSL *ssl = NULL;
	SSL_METHOD *ssl_method = NULL;;
	BIO *bio = NULL;
	if (client)
		ssl_method = (SSL_METHOD*)SSLv23_client_method();
	else
		ssl_method = (SSL_METHOD*)SSLv23_server_method();
	ctx = SSL_CTX_new(ssl_method);
	if (ctx == NULL) {
		mm_tlsio_error(io, 0, "SSL_CTX_new()");
		return -1;
	}

	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
	SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);

	/* verify mode */
	int verify = 0;
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
		if (rc != 1) {
			mm_tlsio_error(io, 0, "SSL_CTX_use_PrivateKey_file()");
			goto error;
		}
	}
	if (tls->cert_file && tls->key_file) {
		rc = SSL_CTX_check_private_key(ctx);
		if(rc != 1) {
			mm_tlsio_error(io, 0, "SSL_CTX_check_private_key()");
			goto error;
		}
	}

	/* ca file and ca_path */
	if (tls->ca_file || tls->ca_path) {
		rc = SSL_CTX_load_verify_locations(ctx, tls->ca_file, tls->ca_path);
		if (rc != 1) {
			mm_tlsio_error(io, 0, "SSL_CTX_load_verify_locations()");
			goto error;
		}
	}

	/* ocsp */

	/* set ciphers */
	const char cipher_list[] =	"ALL:!aNULL:!eNULL";
	rc = SSL_CTX_set_cipher_list(ctx, cipher_list);
	if (rc != 1) {
		mm_tlsio_error(io, 0, "SSL_CTX_set_cipher_list()");
		goto error;
	}
	if (! client)
		SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

	ssl = SSL_new(ctx);
	if (ssl == NULL) {
		mm_tlsio_error(io, 0, "SSL_new()");
		goto error;
	}

	/* set server name */
	if (tls->server) {
		rc = SSL_set_tlsext_host_name(ssl, tls->server);
		if (rc != 1) {
			mm_tlsio_error(io, 0, "SSL_set_tlsext_host_name()");
			goto error;
		}
	}

	bio = BIO_new(&mm_tlsio_method);
	if (bio == NULL) {
		mm_tlsio_error(io, 0, "BIO_new()");
		goto error;
	}
	BIO_set_app_data(bio, io);
	bio->init = 1;
	SSL_set_bio(ssl, bio, bio);

	io->ctx = ctx;
	io->ssl = ssl;
	io->bio = bio;
	return 0;
error:
	SSL_CTX_free(ctx);
	if (ssl)
		SSL_free(ssl);
	if (bio)
		BIO_free(bio);
	return -1;
}

static inline int
mm_tlsio_verify_name(char *cert_name, const char *name)
{
	char *cert_domain, *domain, *next_dot;
	if (strcasecmp(cert_name, name) == 0)
		return 0;

	/* wildcard match */
	if (cert_name[0] != '*')
		return -1;

	/*
	 * valid wildcards:
	 * - "*.domain.tld"
	 * - "*.sub.domain.tld"
	 * - etc.
	 * reject "*.tld".
	 * no attempt to prevent the use of eg. "*.co.uk".
	 */
	cert_domain = &cert_name[1];
	/* disallow "*"  */
	if (cert_domain[0] == '\0')
		return -1;
	/* disallow "*foo" */
	if (cert_domain[0] != '.')
		return -1;
	/* disallow "*.." */
	if (cert_domain[1] == '.')
		return -1;

	next_dot = strchr(&cert_domain[1], '.');
	/* disallow "*.bar" */
	if (next_dot == NULL)
		return -1;
	/* disallow "*.bar.." */
	if (next_dot[1] == '.')
		return -1;

	domain = strchr(name, '.');
	/* no wildcard match against a name with no host part. */
	if (name[0] == '.')
		return -1;
	/* no wildcard match against a name with no domain part. */
	if (domain == NULL || strlen(domain) == 1)
		return -1;

	if (strcasecmp(cert_domain, domain) == 0)
		return 0;

	return -1;
}

static int
mm_tlsio_verify_server_name(mm_tlsio_t *io, mm_tls_t *tls)
{
	X509 *cert = NULL;
	X509_NAME *subject_name = NULL;
	char *common_name = NULL;
	int common_name_len = 0;

	cert = SSL_get_peer_certificate(io->ssl);
	if (cert == NULL) {
		mm_tlsio_error(io, 0, "SSL_get_peer_certificate()");
		return -1;
	}
	subject_name = X509_get_subject_name(cert);
	if (subject_name == NULL) {
		mm_tlsio_error(io, 0, "X509_get_subject_name()");
		goto error;
	}
	common_name_len = X509_NAME_get_text_by_NID(subject_name, NID_commonName, NULL, 0);
	if (common_name_len < 0) {
		mm_tlsio_error(io, 0, "X509_NAME_get_text_by_NID()");
		goto error;
	}
	common_name = calloc(common_name_len + 1, 1);
	if (common_name == NULL) {
		mm_tlsio_error(io, 0, "memory allocation failed");
		goto error;
	}
	X509_NAME_get_text_by_NID(subject_name, NID_commonName,
	                          common_name,
	                          common_name_len + 1);
	/* validate name */
	if (common_name_len != (int)strlen(common_name)) {
		mm_tlsio_error(io, 0, "NUL byte in Common Name field, probably a malicious "
		               "server certificate");
		goto error;
	}
	if (mm_tlsio_verify_name(common_name, tls->server) == -1) {
		mm_tlsio_error(io, 0, "bad server name: %s (expected %s)",
		               common_name, tls->server);
		goto error;
	}
	X509_free(cert);
	return 0;

error:
	X509_free(cert);
	if (common_name)
		free(common_name);
	return -1;
}

static int
mm_tlsio_verify(mm_tlsio_t *io, mm_tls_t *tls)
{
	int rc;
	if (tls->server) {
		rc = mm_tlsio_verify_server_name(io, tls);
		if (rc == -1)
			return -1;
	}
	rc = SSL_get_verify_result(io->ssl);
	if (rc != X509_V_OK) {
		mm_tlsio_error(io, 0, "SSL_get_verify_result()");
		return -1;
	}
	return 0;
}

int mm_tlsio_connect(mm_tlsio_t *io, mm_tls_t *tls)
{
	mm_tlsio_error_reset(io);
	int rc;
	rc = mm_tlsio_prepare(tls, io, 1);
	if (rc == -1)
		return -1;
	rc = SSL_connect(io->ssl);
	if (rc <= 0) {
		mm_tlsio_error(io, rc, "SSL_connect()");
		return -1;
	}
	rc = mm_tlsio_verify(io, tls);
	if (rc == -1)
		return -1;
	return 0;
}

int mm_tlsio_accept(mm_tlsio_t *io, mm_tls_t *tls)
{
	mm_tlsio_error_reset(io);
	int rc;
	rc = mm_tlsio_prepare(tls, io, 0);
	if (rc == -1)
		return -1;
	rc = SSL_accept(io->ssl);
	if (rc <= 0) {
		mm_tlsio_error(io, rc, "SSL_accept()");
		return -1;
	}
	return 0;
}

int mm_tlsio_close(mm_tlsio_t *io)
{
	mm_tlsio_error_reset(io);
	(void)io;
	return 0;
}

int mm_tlsio_write(mm_tlsio_t *io, char *buf, int size, uint32_t time_ms)
{
	mm_tlsio_error_reset(io);
	io->time_ms = time_ms;
	int rc;
	rc = SSL_write(io->ssl, buf, size);
	if (rc <= 0) {
		mm_tlsio_error(io, rc, "SSL_write()");
		return -1;
	}
	return 0;
}

int mm_tlsio_read(mm_tlsio_t *io, char *buf, int size, uint32_t time_ms)
{
	mm_tlsio_error_reset(io);
	io->time_ms = time_ms;
	int rc;
	rc = SSL_read(io->ssl, buf, size);
	if (rc <= 0) {
		mm_tlsio_error(io, rc, "SSL_read()");
		return -1;
	}
	return 0;
}
