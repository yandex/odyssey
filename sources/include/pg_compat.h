#pragma once

/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

/*-------------------------------------------------------------------------
 *
 * c.h
 *	  Fundamental C definitions.  This is included by every .c file in
 *	  PostgreSQL (via either postgres.h or postgres_fe.h, as appropriate).
 *
 *	  Note that the definitions here are not intended to be exposed to clients
 *	  of the frontend interface libraries --- so we don't worry much about
 *	  polluting the namespace with lots of stuff...
 *
 *
 * Portions Copyright (c) 1996-2025, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/c.h
 *
 *-------------------------------------------------------------------------
 */

/* various things to use postgresql source files at odyssey */

#define int8 int8_t
#define uint8 uint8_t
#define uint16 uint16_t
#define uint32 uint32_t
#define uint64 uint64_t
#define int64 int64_t

typedef uint16_t char16_t;
typedef uint32_t char32_t;

typedef size_t Size;

#define Assert(p) assert(p)
#define AssertMacro(p) ((void)assert(p))

#define lengthof(array) (sizeof(array) / sizeof((array)[0]))

#define pg_attribute_noreturn() _NORETURN

#define gettext(x) (x)
#define _(x) gettext(x)

#ifndef pg_nodiscard
#define pg_nodiscard __attribute__((warn_unused_result))
#endif

/*
 * pg_noreturn corresponds to the C11 noreturn/_Noreturn function specifier.
 * We can't use the standard name "noreturn" because some third-party code
 * uses __attribute__((noreturn)) in headers, which would get confused if
 * "noreturn" is defined to "_Noreturn", as is done by <stdnoreturn.h>.
 *
 * In a declaration, function specifiers go before the function name.  The
 * common style is to put them before the return type.  (The MSVC fallback has
 * the same requirement.  The GCC fallback is more flexible.)
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define pg_noreturn _Noreturn
#elif defined(__GNUC__)
#define pg_noreturn __attribute__((noreturn))
#elif defined(_MSC_VER)
#define pg_noreturn __declspec(noreturn)
#else
#define pg_noreturn
#endif

/* only GCC supports the unused attribute */
#ifdef __GNUC__
#define pg_attribute_unused() __attribute__((unused))
#else
#define pg_attribute_unused()
#endif

/* GCC supports format attributes */
#if defined(__GNUC__)
#define pg_attribute_format_arg(a) __attribute__((format_arg(a)))
#define pg_attribute_printf(f, a) \
	__attribute__((format(PG_PRINTF_ATTRIBUTE, f, a)))
#else
#define pg_attribute_format_arg(a)
#define pg_attribute_printf(f, a)
#endif

#define PG_C_PRINTF_ATTRIBUTE printf

#ifndef __cplusplus
#define PG_PRINTF_ATTRIBUTE PG_C_PRINTF_ATTRIBUTE
#else
#define PG_PRINTF_ATTRIBUTE PG_CXX_PRINTF_ATTRIBUTE
#endif

/*
 * Append PG_USED_FOR_ASSERTS_ONLY to definitions of variables that are only
 * used in assert-enabled builds, to avoid compiler warnings about unused
 * variables in assert-disabled builds.
 */
#ifdef USE_ASSERT_CHECKING
#define PG_USED_FOR_ASSERTS_ONLY
#else
#define PG_USED_FOR_ASSERTS_ONLY pg_attribute_unused()
#endif

#define PGDLLIMPORT

/*
 * MemSet
 *	just repplaced with stdlib memset
 *	while original pg MemSet have some optimizations
 */
#define MemSet(start, val, len) memset((start), (val), (len))

#define INT64CONST(x) INT64_C(x)
#define UINT64CONST(x) UINT64_C(x)

/* msb for char */
#define HIGHBIT (0x80)
#define IS_HIGHBIT_SET(ch) ((unsigned char)(ch) & HIGHBIT)

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
