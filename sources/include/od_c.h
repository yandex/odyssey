#pragma once

/*
 * fundamental and common C definitions
 * should be included in every .c file by odyssey.h
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <math.h>

#include <pthread.h>

#define od_likely(EXPR) __builtin_expect(!!(EXPR), 1)
#define od_unlikely(EXPR) __builtin_expect(!!(EXPR), 0)

#if defined(__linux__)
#define od_read_mostly __attribute__((__section__(".data.read_mostly")))
#else
#define od_read_mostly
#endif

#define od_container_of(N, T, F) ((T *)((char *)(N) - __builtin_offsetof(T, F)))

#ifndef lengthof
#define lengthof(array) (sizeof(array) / sizeof((array)[0]))
#endif

#define OK_RESPONSE 0
#define NOT_OK_RESPONSE -1

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)
#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)

static const uint64_t interval_usec = 1000000ull;

typedef int od_retcode_t;

#define INVALID_COROUTINE_ID -1

/* only GCC supports the unused attribute */
#ifdef __GNUC__
#define od_attribute_unused() __attribute__((unused))
#else
#define od_attribute_unused()
#endif

/* GCC support aligned, packed and noreturn */
#ifdef __GNUC__
#define od_attribute_aligned(a) __attribute__((aligned(a)))
#define od_attribute_noreturn() __attribute__((noreturn))
#define od_attribute_packed() __attribute__((packed))
#endif

#define FLEXIBLE_ARRAY_MEMBER /* empty */

#if defined __has_builtin
#if __has_builtin(__builtin_unreachable) /* odyssey unreachable code */
#define od_unreachable() __builtin_unreachable()
#endif
#endif

#ifndef od_unreachable
#define od_unreachable() abort()
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define OD_THREAD_LOCAL thread_local
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define OD_THREAD_LOCAL _Thread_local
#elif defined(_MSC_VER)
#define OD_THREAD_LOCAL __declspec(thread)
#else
#define OD_THREAD_LOCAL __thread
#endif

#if defined(__linux__)

#include <endian.h>

#elif defined(__APPLE__)

#include <libkern/OSByteOrder.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)

#include <sys/endian.h>

#elif defined(__OpenBSD__)

#include <sys/types.h>
#include <sys/endian.h>

/* OpenBSD historically lacks the *toh names in some versions */
#ifndef be16toh
#define be16toh(x) betoh16(x)
#define le16toh(x) letoh16(x)
#define be32toh(x) betoh32(x)
#define le32toh(x) letoh32(x)
#define be64toh(x) betoh64(x)
#define le64toh(x) letoh64(x)
#endif

#else
#error "machinarium: unsupported platform, no endian.h replacement available"
#endif

static inline void od_explicit_bzero(void *buf, size_t len)
{
	memset(buf, 0, len);
	/* prevent the compiler from optimizing away the memset above */
	__asm__ __volatile__("" : : "r"(buf) : "memory");
}

#if !defined(explicit_bzero)
#define explicit_bzero od_explicit_bzero
#endif
