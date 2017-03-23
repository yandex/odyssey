/* $OpenBSD: tls_internal.h,v 1.11 2015/02/22 14:50:41 jsing Exp $ */
/*
 * Copyright (c) 2014 Jeremie Courreges-Anglas <jca@openbsd.org>
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HEADER_TLS_INTERNAL_H
#define HEADER_TLS_INTERNAL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <openssl/ssl.h>

#define _PATH_SSL_CA_FILE "/etc/ssl/certs/ca-certificates.crt"

/*
 * Anything that is not completely broken.
 *
 * Also fixes 3DES ordering bug in older OpenSSLs.
 */
#define TLS_CIPHERS_COMPAT	"HIGH:+3DES:!aNULL"

/*
 * All==insecure.
 */
#define TLS_CIPHERS_ALL		"ALL:!aNULL:!eNULL"

/*
 * TLSv1.2+AEAD+ECDHE/DHE.  CBC modes are dubious due to spec bugs in TLS.
 */
#define TLS_CIPHERS_DEFAULT	"HIGH+EECDH:HIGH+EDH:!SSLv3:!SHA384:!SHA256:!DSS:!aNULL"

/*
 * Compact subset of reasonable suites only.
 *
 * Priorities, in order:
 * - ECDHE > DHE > RSA
 * - AESGCM > CBC
 * - TLSv1.2 > TLSv1.0
 * - AES256 > AES128.
 */
#define TLS_CIPHERS_NORMAL	"HIGH+EECDH:HIGH+EDH:HIGH+RSA:+SHA384:+SHA256:+SSLv3:+EDH:+RSA:-3DES:3DES+RSA:!CAMELLIA:!DSS:!aNULL"

/*
 * Prefer performance if it does not affect security.
 *
 * Same as "normal" but prefers AES128 to AES256.
 */
#define TLS_CIPHERS_FAST	"HIGH+EECDH:HIGH+EDH:HIGH+RSA:+AES256:+SHA256:+SHA384:+SSLv3:+EDH:+RSA:-3DES:3DES+RSA:!CAMELLIA:!DSS:!aNULL"

union tls_addr {
	struct in_addr ip4;
	struct in6_addr ip6;
};

struct tls_config {
	const char *ca_file;
	const char *ca_path;
	char *ca_mem;
	size_t ca_len;
	const char *cert_file;
	char *cert_mem;
	size_t cert_len;
	const char *ciphers;
	int ciphers_server;
	int dheparams;
	int ecdhecurve;
	const char *key_file;
	char *key_mem;
	size_t key_len;
	const char *ocsp_file;
	char *ocsp_mem;
	size_t ocsp_len;
	uint32_t protocols;
	int verify_cert;
	int verify_client;
	int verify_depth;
	int verify_name;
	int verify_time;
};

struct tls_conninfo {
	char *issuer;
	char *subject;
	char *hash;
	char *serial;
	char *fingerprint;
	char *version;
	char *cipher;
	time_t notbefore;
	time_t notafter;
};

#define TLS_CLIENT		(1 << 0)
#define TLS_SERVER		(1 << 1)
#define TLS_SERVER_CONN		(1 << 2)
#define TLS_OCSP_CLIENT		(1 << 3)

#define TLS_EOF_NO_CLOSE_NOTIFY	(1 << 0)
#define TLS_HANDSHAKE_COMPLETE	(1 << 1)
#define TLS_DO_ABORT		(1 << 8)

struct tls_ocsp_query;
struct tls_ocsp_info;

struct tls {
	struct tls_config *config;
	uint32_t flags;
	uint32_t state;

	char *errmsg;
	int errnum;

	char *servername;
	int socket;

	SSL *ssl_conn;
	SSL_CTX *ssl_ctx;
	X509 *ssl_peer_cert;
	struct tls_conninfo *conninfo;

	int used_dh_bits;
	int used_ecdh_nid;

	const char *ocsp_result;
	struct tls_ocsp_info *ocsp_info;

	struct tls_ocsp_query *ocsp_query;
};

struct tls_ocsp_info {
	int response_status;
	int cert_status;
	int crl_reason;
	time_t this_update;
	time_t next_update;
	time_t revocation_time;
};

struct tls *tls_new(void);
struct tls *tls_server_conn(struct tls *ctx);

int tls_check_name(struct tls *ctx, X509 *cert, const char *servername);
int tls_configure_keypair(struct tls *ctx, int);
int tls_configure_server(struct tls *ctx);
int tls_configure_ssl(struct tls *ctx);
int tls_configure_ssl_verify(struct tls *ctx, int verify);
int tls_handshake_client(struct tls *ctx);
int tls_handshake_server(struct tls *ctx);
int tls_host_port(const char *hostport, char **host, char **port);
int tls_set_error(struct tls *ctx, const char *fmt, ...)
    __attribute__((__format__ (printf, 2, 3)))
    __attribute__((__nonnull__ (2)));
int tls_set_errorx(struct tls *ctx, const char *fmt, ...)
    __attribute__((__format__ (printf, 2, 3)))
    __attribute__((__nonnull__ (2)));
int tls_set_error_libssl(struct tls *ctx, const char *fmt, ...)
    __attribute__((__format__ (printf, 2, 3)))
    __attribute__((__nonnull__ (2)));
int tls_ssl_error(struct tls *ctx, SSL *ssl_conn, int ssl_ret,
    const char *prefix);

int tls_get_conninfo(struct tls *ctx);
void tls_free_conninfo(struct tls_conninfo *conninfo);

int tls_ocsp_verify_callback(SSL *ssl, void *arg);
int tls_ocsp_stapling_callback(SSL *ssl, void *arg);
void tls_ocsp_client_free(struct tls *ctx);
void tls_ocsp_info_free(struct tls_ocsp_info *info);

int tls_asn1_parse_time(struct tls *ctx, const ASN1_TIME *asn1time, time_t *dst);

int asn1_time_parse(const char *, size_t, struct tm *, int);

#endif /* HEADER_TLS_INTERNAL_H */
