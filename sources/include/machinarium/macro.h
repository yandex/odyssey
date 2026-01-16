#pragma once

/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#define mm_container_of(ptr, type, field) \
	((type *)((char *)(ptr) - __builtin_offsetof(type, field)))

#define mm_cast(type, ptr) ((type)(ptr))

typedef int mm_retcode_t;

#define MM_OK_RETCODE 0
#define MM_NOTOK_RETCODE -1

#ifndef IOV_MAX
#define IOV_MAX __IOV_MAX
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MM_THREAD_LOCAL thread_local
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define MM_THREAD_LOCAL _Thread_local
#elif defined(_MSC_VER)
#define MM_THREAD_LOCAL __declspec(thread)
#else
#define MM_THREAD_LOCAL __thread
#endif

#define mm_likely(EXPR) __builtin_expect(!!(EXPR), 1)
#define mm_unlikely(EXPR) __builtin_expect(!!(EXPR), 0)
