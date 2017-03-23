/*
 * Compatibility with various libssl implementations.
 *
 * Copyright (c) 2015  Marko Kreen
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include <openssl/err.h>
#include <openssl/dh.h>

#include "tls_internal.h"
#include "tls_compat.h"
#include "tls.h"

#ifndef SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE
#undef SSLerr
#undef X509err
#endif

#ifndef SSLerr
#define SSLerr(a,b) do {} while (0)
#define X509err(a,b) do {} while (0)
#endif

#ifndef SSL_CTX_set_dh_auto
#define DH_CLEANUP

/*
 * SKIP primes, used by OpenSSL and PostgreSQL.
 *
 * https://tools.ietf.org/html/draft-ietf-ipsec-skip-06
 */

static const char file_dh1024[] =
"-----BEGIN DH PARAMETERS-----\n"
"MIGHAoGBAPSI/VhOSdvNILSd5JEHNmszbDgNRR0PfIizHHxbLY7288kjwEPwpVsY\n"
"jY67VYy4XTjTNP18F1dDox0YbN4zISy1Kv884bEpQBgRjXyEpwpy1obEAxnIByl6\n"
"ypUM2Zafq9AKUJsCRtMIPWakXUGfnHy9iUsiGSa6q6Jew1XpL3jHAgEC\n"
"-----END DH PARAMETERS-----\n";

static const char file_dh2048[] =
"-----BEGIN DH PARAMETERS-----\n"
"MIIBCAKCAQEA9kJXtwh/CBdyorrWqULzBej5UxE5T7bxbrlLOCDaAadWoxTpj0BV\n"
"89AHxstDqZSt90xkhkn4DIO9ZekX1KHTUPj1WV/cdlJPPT2N286Z4VeSWc39uK50\n"
"T8X8dryDxUcwYc58yWb/Ffm7/ZFexwGq01uejaClcjrUGvC/RgBYK+X0iP1YTknb\n"
"zSC0neSRBzZrM2w4DUUdD3yIsxx8Wy2O9vPJI8BD8KVbGI2Ou1WMuF040zT9fBdX\n"
"Q6MdGGzeMyEstSr/POGxKUAYEY18hKcKctaGxAMZyAcpesqVDNmWn6vQClCbAkbT\n"
"CD1mpF1Bn5x8vYlLIhkmuquiXsNV6TILOwIBAg==\n"
"-----END DH PARAMETERS-----\n";

static const char file_dh4096[] =
"-----BEGIN DH PARAMETERS-----\n"
"MIICCAKCAgEA+hRyUsFN4VpJ1O8JLcCo/VWr19k3BCgJ4uk+d+KhehjdRqNDNyOQ\n"
"l/MOyQNQfWXPeGKmOmIig6Ev/nm6Nf9Z2B1h3R4hExf+zTiHnvVPeRBhjdQi81rt\n"
"Xeoh6TNrSBIKIHfUJWBh3va0TxxjQIs6IZOLeVNRLMqzeylWqMf49HsIXqbcokUS\n"
"Vt1BkvLdW48j8PPv5DsKRN3tloTxqDJGo9tKvj1Fuk74A+Xda1kNhB7KFlqMyN98\n"
"VETEJ6c7KpfOo30mnK30wqw3S8OtaIR/maYX72tGOno2ehFDkq3pnPtEbD2CScxc\n"
"alJC+EL7RPk5c/tgeTvCngvc1KZn92Y//EI7G9tPZtylj2b56sHtMftIoYJ9+ODM\n"
"sccD5Piz/rejE3Ome8EOOceUSCYAhXn8b3qvxVI1ddd1pED6FHRhFvLrZxFvBEM9\n"
"ERRMp5QqOaHJkM+Dxv8Cj6MqrCbfC4u+ZErxodzuusgDgvZiLF22uxMZbobFWyte\n"
"OvOzKGtwcTqO/1wV5gKkzu1ZVswVUQd5Gg8lJicwqRWyyNRczDDoG9jVDxmogKTH\n"
"AaqLulO7R8Ifa1SwF2DteSGVtgWEN8gDpN3RBmmPTDngyF2DHb5qmpnznwtFKdTL\n"
"KWbuHn491xNO25CQWMtem80uKw+pTnisBRF/454n1Jnhub144YRBoN8CAQI=\n"
"-----END DH PARAMETERS-----\n";


static DH *dh1024, *dh2048, *dh4096;

static DH *load_dh_buffer(struct tls *ctx, DH **dhp, const char *buf)
{
	BIO *bio;
	DH *dh = *dhp;
	if (dh == NULL) {
		bio = BIO_new_mem_buf((char *)buf, strlen(buf));
		if (bio) {
			dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
			BIO_free(bio);
		}
		*dhp = dh;
	}
	if (ctx)
		ctx->used_dh_bits = DH_size(dh) * 8;
	return dh;
}

static DH *dh_auto_cb(SSL *s, int is_export, int keylength)
{
	EVP_PKEY *pk;
	int bits;
	struct tls *ctx = SSL_get_app_data(s);

	pk = SSL_get_privatekey(s);
	if (!pk)
		return load_dh_buffer(ctx, &dh2048, file_dh2048);

	bits = EVP_PKEY_bits(pk);
	if (bits >= 3072)
		return load_dh_buffer(ctx, &dh4096, file_dh4096);
	if (bits >= 1536)
		return load_dh_buffer(ctx, &dh2048, file_dh2048);
	return load_dh_buffer(ctx, &dh1024, file_dh1024);
}

static DH *dh_legacy_cb(SSL *s, int is_export, int keylength)
{
	struct tls *ctx = SSL_get_app_data(s);
	return load_dh_buffer(ctx, &dh1024, file_dh1024);
}

long SSL_CTX_set_dh_auto(SSL_CTX *ctx, int onoff)
{
	if (onoff == 0)
		return 1;
	if (onoff == 2) {
		SSL_CTX_set_tmp_dh_callback(ctx, dh_legacy_cb);
	} else {
		SSL_CTX_set_tmp_dh_callback(ctx, dh_auto_cb);
	}
	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
	return 1;
}

#endif

#ifndef SSL_CTX_set_ecdh_auto
#define ECDH_CLEANUP

/*
 * Use same curve as EC key, fallback to NIST P-256.
 */

static EC_KEY *ecdh_cache;

#ifdef USE_LIBSSL_INTERNALS
static EC_KEY *ecdh_auto_cb(SSL *ssl, int is_export, int keylength)
{
	int last_nid;
	int nid = 0;
	EVP_PKEY *pk;
	EC_KEY *ec;
	struct tls *ctx = SSL_get_app_data(ssl);

	/* pick curve from EC key */
	pk = SSL_get_privatekey(ssl);
	if (pk && EVP_PKEY_id(pk) == EVP_PKEY_EC) {
		ec = EVP_PKEY_get1_EC_KEY(pk);
		if (ec) {
			nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec));
			EC_KEY_free(ec);
		}
	}

	/* ssl->tlsext_ellipticcurvelist is empty, nothing else to do... */
	if (nid == 0)
		nid = NID_X9_62_prime256v1;

	if (ctx)
		ctx->used_ecdh_nid = nid;

	if (ecdh_cache) {
		last_nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ecdh_cache));
		if (last_nid == nid)
			return ecdh_cache;
		EC_KEY_free(ecdh_cache);
		ecdh_cache = NULL;
	}

	ecdh_cache = EC_KEY_new_by_curve_name(nid);
	return ecdh_cache;
}
#endif

long SSL_CTX_set_ecdh_auto(SSL_CTX *ctx, int onoff)
{
	if (onoff) {
		SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);
#ifdef USE_LIBSSL_INTERNALS
		SSL_CTX_set_tmp_ecdh_callback(ctx, ecdh_auto_cb);
#endif
	}
	return 1;
}

#endif

void tls_compat_cleanup(void)
{
#ifdef DH_CLEANUP
	if (dh1024) { DH_free(dh1024); dh1024 = NULL; }
	if (dh2048) { DH_free(dh2048); dh2048 = NULL; }
	if (dh4096) { DH_free(dh4096); dh4096 = NULL; }
#endif
#ifdef ECDH_CLEANUP
	if (ecdh_cache) {
		EC_KEY_free(ecdh_cache);
		ecdh_cache = NULL;
	}
#endif
}

#ifndef HAVE_SSL_CTX_USE_CERTIFICATE_CHAIN_MEM

/*
 * Load certs for public key from memory.
 */

int
SSL_CTX_use_certificate_chain_mem(SSL_CTX *ctx, void *data, int data_len)
{
	pem_password_cb *psw_fn = NULL;
	void *psw_arg = NULL;
	X509 *cert;
	BIO *bio = NULL;
	int ok;

#ifdef USE_LIBSSL_INTERNALS
	psw_fn = ctx->default_passwd_callback;
	psw_arg = ctx->default_passwd_callback_userdata;
#endif

	ERR_clear_error();

	/* Read from memory */
	bio = BIO_new_mem_buf(data, data_len);
	if (!bio) {
		SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE, ERR_R_BUF_LIB);
		goto failed;
	}

	/* Load primary cert */
	cert = PEM_read_bio_X509_AUX(bio, NULL, psw_fn, psw_arg);
	if (!cert) {
		SSLerr(SSL_F_SSL_CTX_USE_CERTIFICATE_CHAIN_FILE, ERR_R_PEM_LIB);
		goto failed;
	}

	/* Increments refcount */
	ok = SSL_CTX_use_certificate(ctx, cert);
	X509_free(cert);
	if (!ok || ERR_peek_error())
		goto failed;

	/* Load extra certs */
	ok = SSL_CTX_clear_extra_chain_certs(ctx);
	while (ok) {
		cert = PEM_read_bio_X509(bio, NULL, psw_fn, psw_arg);
		if (!cert) {
			/* Is it EOF? */
			unsigned long err = ERR_peek_last_error();
			if (ERR_GET_LIB(err) != ERR_LIB_PEM)
				break;
			if (ERR_GET_REASON(err) != PEM_R_NO_START_LINE)
				break;

			/* On EOF do successful exit */
			BIO_free(bio);
			ERR_clear_error();
			return 1;
		}
		/* Does not increment refcount */
		ok = SSL_CTX_add_extra_chain_cert(ctx, cert);
		if (!ok)
			X509_free(cert);
	}
 failed:
	if (bio)
		BIO_free(bio);
	return 0;
}

#endif

#ifndef HAVE_SSL_CTX_LOAD_VERIFY_MEM

/*
 * Load CA certs for verification from memory.
 */

int SSL_CTX_load_verify_mem(SSL_CTX *ctx, void *data, int data_len)
{
	STACK_OF(X509_INFO) *stack = NULL;
	X509_STORE *store;
	X509_INFO *info;
	int nstack, i, ret = 0, got = 0;
	BIO *bio;

	/* Read from memory */
	bio = BIO_new_mem_buf(data, data_len);
	if (!bio)
		goto failed;

	/* Parse X509_INFO records */
	stack = PEM_X509_INFO_read_bio(bio, NULL, NULL, NULL);
	if (!stack)
		goto failed;

	/* Loop over stack, add certs and revocation records to store */
	store = SSL_CTX_get_cert_store(ctx);
	nstack = sk_X509_INFO_num(stack);
	for (i = 0; i < nstack; i++) {
		info = sk_X509_INFO_value(stack, i);
		if (info->x509 && !X509_STORE_add_cert(store, info->x509))
			goto failed;
		if (info->crl && !X509_STORE_add_crl(store, info->crl))
			goto failed;
		if (info->x509 || info->crl)
			got = 1;
	}
	ret = got;
 failed:
	if (bio)
		BIO_free(bio);
	if (stack)
		sk_X509_INFO_pop_free(stack, X509_INFO_free);
	if (!ret)
		X509err(X509_F_X509_LOAD_CERT_CRL_FILE, ERR_R_PEM_LIB);
	return ret;
}

#endif

#ifndef HAVE_ASN1_TIME_PARSE

static int
parse2num(const char **str_p, int min, int max)
{
	const char *s = *str_p;
	if (s && s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9') {
		int val = (s[0] - '0') * 10 + (s[1] - '0');
		if (val >= min && val <= max) {
			*str_p += 2;
			return val;
		}
	}
	*str_p = NULL;
	return 0;
}

int
asn1_time_parse(const char *src, size_t len, struct tm *tm, int mode)
{
	char buf[16];
	const char *s = buf;
	int utctime;
	int year;

	memset(tm, 0, sizeof *tm);

	if (mode != 0)
		return -1;

	/*
	 * gentime: YYYYMMDDHHMMSSZ
	 * utctime: YYMMDDHHMM(SS)(Z)
	 */
	if (len == 15) {
		utctime = 0;
	} else if (len > 8 && len < 15) {
		utctime = 1;
	} else {
		return -1;
	}
	memcpy(buf, src, len);
	buf[len] = '\0';

	year = parse2num(&s, 0, 99);
	if (utctime) {
		if (year < 50)
			year = 2000 + year;
		else
			year = 1900 + year;
	} else {
		year = year*100 + parse2num(&s, 0, 99);
	}
	tm->tm_year = year - 1900;
	tm->tm_mon = parse2num(&s, 1, 12) - 1;
	tm->tm_mday = parse2num(&s, 1, 31);
	tm->tm_hour = parse2num(&s, 0, 23);
	tm->tm_min = parse2num(&s, 0, 59);
	if (utctime) {
		if (s && s[0] != 'Z' && s[0] != '\0')
			tm->tm_sec = parse2num(&s, 0, 61);
	} else {
		tm->tm_sec = parse2num(&s, 0, 61);
	}

	if (s) {
		if (s[0] == '\0')
			goto good;
		if (s[0] == 'Z' && s[1] == '\0')
			goto good;
	}
	return -1;
 good:
	return utctime ? V_ASN1_UTCTIME : V_ASN1_GENERALIZEDTIME;
}

#endif /* HAVE_ASN1_TIME_PARSE */

int
tls_asn1_parse_time(struct tls *ctx, const ASN1_TIME *asn1time, time_t *dst)
{
	struct tm tm;
	int res;
	time_t tval;

	*dst = 0;
	if (!asn1time)
		return 0;
	if (asn1time->type != V_ASN1_GENERALIZEDTIME &&
	    asn1time->type != V_ASN1_UTCTIME) {
		tls_set_errorx(ctx, "Invalid time object type: %d", asn1time->type);
		return -1;
	}

	res = asn1_time_parse((char*)asn1time->data, asn1time->length, &tm, 0);
	if (res == -1) {
		tls_set_errorx(ctx, "Invalid asn1 time");
		return -1;
	}

	tval = timegm(&tm);
	if (tval == (time_t)-1) {
		tls_set_error(ctx, "Cannot convert asn1 time");
		return -1;
	}
	*dst = tval;
	return 0;
}
