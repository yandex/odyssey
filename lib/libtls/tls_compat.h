#ifndef _TLS_COMPAT_H_
#define _TLS_COMPAT_H_

#include <openssl/ssl.h>

/* OpenSSL 1.1+ has hidden struct fields */
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

#define USE_LIBSSL_INTERNALS

#define X509_get_key_usage(x509) ((x509)->ex_kusage)
#define X509_get_extended_key_usage(x509) ((x509)->ex_xkusage)
#define SSL_CTX_get0_param(ssl_ctx) ((ssl_ctx)->param)
#endif

/* ecdh_auto is broken - ignores main EC key */
#undef SSL_CTX_set_ecdh_auto

/* dh_auto seems fine, but use ours to get DH info */
#undef SSL_CTX_set_dh_auto

#ifndef SSL_CTX_set_dh_auto
long SSL_CTX_set_dh_auto(SSL_CTX *ctx, int onoff);
#endif

#ifndef SSL_CTX_set_ecdh_auto
long SSL_CTX_set_ecdh_auto(SSL_CTX *ctx, int onoff);
#endif

#ifndef HAVE_SSL_CTX_USE_CERTIFICATE_CHAIN_MEM
int SSL_CTX_use_certificate_chain_mem(SSL_CTX *ctx, void *buf, int len);
#endif

#ifndef HAVE_SSL_CTX_LOAD_VERIFY_MEM
int SSL_CTX_load_verify_mem(SSL_CTX *ctx, void *buf, int len);
#endif

/* BoringSSL has no OCSP support */
#ifdef OPENSSL_IS_BORINGSSL
#define SSL_CTX_set_tlsext_status_cb(a,b) (1)
#define SSL_set_tlsext_status_type(a,b) (1)
#endif

void tls_compat_cleanup(void);

#endif

