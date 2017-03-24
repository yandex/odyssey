
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

int mm_tls_client(void)
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
	/*
	SSL_CTX_set_verify()
	SSL_CTX_set_verify_depth()
	SSL_CTX_set_options()
	SSL_CTX_load_verify_locations()
	*/
	(void)ssl;
	(void)bio;
	return 0;
}
