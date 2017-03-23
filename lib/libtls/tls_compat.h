#ifndef _TLS_COMPAT_H_
#define _TLS_COMPAT_H_

#include <openssl/ssl.h>

#ifndef TLS_MAX_SESSION_ID_LENGTH
#define TLS_MAX_SESSION_ID_LENGTH 32
#endif

#ifndef HAVE_SSL_CTX_USE_CERTIFICATE_CHAIN_MEM
int SSL_CTX_use_certificate_chain_mem(SSL_CTX *ctx, void *buf, int len);
#endif

#ifndef HAVE_SSL_CTX_LOAD_VERIFY_MEM
int SSL_CTX_load_verify_mem(SSL_CTX *ctx, void *buf, int len);
#endif

#ifndef SSL_CTX_set_dh_auto
long SSL_CTX_set_dh_auto(SSL_CTX *ctx, int onoff);
#endif

int compat_ASN1_time_parse(const char *src, size_t len, struct tm *tm, int mode);
int compat_timingsafe_memcmp(const void *b1, const void *b2, size_t len);

#endif

