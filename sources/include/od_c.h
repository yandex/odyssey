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
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <math.h>

#define od_likely(EXPR) __builtin_expect(!!(EXPR), 1)
#define od_unlikely(EXPR) __builtin_expect(!!(EXPR), 0)

#define od_read_mostly __attribute__((__section__(".data.read_mostly")))

#define od_container_of(N, T, F) ((T *)((char *)(N) - __builtin_offsetof(T, F)))

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
