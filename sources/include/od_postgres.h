#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#define int8 int8_t
#define uint8 uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define uint64 uint64_t

#define lengthof(array) (sizeof(array) / sizeof((array)[0]))
#define pg_hton32(x) htobe32(x)

#define pg_attribute_noreturn() _NORETURN

#ifndef pg_nodiscard
#define pg_nodiscard __attribute__((warn_unused_result))
#endif

#define FRONTEND

#include <pg_config.h>

/*
 * Undefine OPENSSL_API_COMPAT to prevent conflicts with system OpenSSL headers.
 * PostgreSQL's pg_config.h defines this, but on OpenSSL 1.1.x, the system headers
 * define it differently (as OPENSSL_MIN_API), causing redefinition errors.
 * This is only needed for OpenSSL < 3.0.
 */
#include <openssl/opensslv.h>
#if defined(OPENSSL_API_COMPAT) && OPENSSL_VERSION_NUMBER < 0x30000000L
#undef OPENSSL_API_COMPAT
#endif

#include <string.h>

#include <common/base64.h>
#include <common/saslprep.h>

#include <common/scram-common.h>

#if PG_VERSION_NUM >= 140000
#include <common/hmac.h>
#endif
